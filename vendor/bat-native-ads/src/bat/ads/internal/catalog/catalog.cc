/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/catalog/catalog.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "bat/ads/ads.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/base/http_status_code.h"
#include "bat/ads/internal/base/logging_util.h"
#include "bat/ads/internal/base/time_formatting_util.h"
#include "bat/ads/internal/catalog/catalog_constants.h"
#include "bat/ads/internal/catalog/catalog_info.h"
#include "bat/ads/internal/catalog/catalog_json_reader.h"
#include "bat/ads/internal/catalog/catalog_url_request_builder.h"
#include "bat/ads/internal/catalog/catalog_util.h"
#include "bat/ads/internal/server/url/url_request_string_util.h"
#include "bat/ads/internal/server/url/url_response_string_util.h"

namespace ads {

namespace {

constexpr base::TimeDelta kRetryAfter = base::Minutes(1);

constexpr base::TimeDelta kDebugCatalogPing = base::Minutes(15);

}  // namespace

Catalog::Catalog() = default;

Catalog::~Catalog() = default;

void Catalog::AddObserver(CatalogObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void Catalog::RemoveObserver(CatalogObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void Catalog::MaybeFetch() {
  if (is_processing_ || retry_timer_.IsRunning()) {
    return;
  }

  Fetch();
}

///////////////////////////////////////////////////////////////////////////////

void Catalog::Fetch() {
  DCHECK(!is_processing_);

  BLOG(1, "Catalog");
  BLOG(2, "GET /v" << kCatalogVersion << "/catalog");

  is_processing_ = true;

  CatalogUrlRequestBuilder url_request_builder;
  mojom::UrlRequestPtr url_request = url_request_builder.Build();
  BLOG(6, UrlRequestToString(url_request));
  BLOG(7, UrlRequestHeadersToString(url_request));

  const auto callback =
      std::bind(&Catalog::OnFetch, this, std::placeholders::_1);
  AdsClientHelper::Get()->UrlRequest(std::move(url_request), callback);
}

void Catalog::OnFetch(const mojom::UrlResponse& url_response) {
  BLOG(1, "OnCatalog");

  BLOG(7, UrlResponseToString(url_response));
  BLOG(7, UrlResponseHeadersToString(url_response));

  is_processing_ = false;

  if (url_response.status_code == net::HTTP_NOT_MODIFIED) {
    BLOG(1, "Catalog is up to date");
    FetchAfterDelay();
    return;
  } else if (url_response.status_code == net::HTTP_UPGRADE_REQUIRED) {
    BLOG(1, "Failed to fetch catalog as a browser upgrade is required");
    NotifyFailedToUpdateCatalog();
    return;
  } else if (url_response.status_code != net::HTTP_OK) {
    BLOG(1, "Failed to fetch catalog");
    NotifyFailedToUpdateCatalog();
    Retry();
  }

  BLOG(1, "Successfully fetched catalog");

  BLOG(1, "Parsing catalog");
  const absl::optional<CatalogInfo> catalog_optional =
      JSONReader::ReadCatalog(url_response.body);
  if (!catalog_optional) {
    BLOG(1, "Failed to parse catalog");
    NotifyFailedToUpdateCatalog();
    Retry();
    return;
  }
  const CatalogInfo& catalog = catalog_optional.value();

  if (catalog.version != kCatalogVersion) {
    BLOG(1, "Catalog version mismatch");
    NotifyFailedToUpdateCatalog();
    Retry();
    return;
  }

  SetCatalogLastUpdated(base::Time::Now());

  if (!HasCatalogChanged(catalog.id)) {
    BLOG(1, "Catalog id " << catalog.id << " is up to date");
    FetchAfterDelay();
    return;
  }

  SaveCatalog(catalog);
  NotifyDidUpdateCatalog(catalog);
  FetchAfterDelay();
}

void Catalog::FetchAfterDelay() {
  retry_timer_.Stop();

  const base::TimeDelta delay =
      g_is_debug ? kDebugCatalogPing : GetCatalogPing();

  const base::Time fetch_at = timer_.StartWithPrivacy(
      delay, base::BindOnce(&Catalog::Fetch, base::Unretained(this)));

  BLOG(1, "Fetch catalog " << FriendlyDateAndTime(fetch_at));
}

void Catalog::Retry() {
  const base::Time time = retry_timer_.StartWithPrivacy(
      kRetryAfter, base::BindOnce(&Catalog::OnRetry, base::Unretained(this)));

  BLOG(1, "Retry fetching catalog " << FriendlyDateAndTime(time));
}

void Catalog::OnRetry() {
  BLOG(1, "Retry fetching catalog");

  Fetch();
}

void Catalog::NotifyDidUpdateCatalog(const CatalogInfo& catalog) const {
  for (CatalogObserver& observer : observers_) {
    observer.OnDidUpdateCatalog(catalog);
  }
}

void Catalog::NotifyFailedToUpdateCatalog() const {
  for (CatalogObserver& observer : observers_) {
    observer.OnFailedToUpdateCatalog();
  }
}

}  // namespace ads