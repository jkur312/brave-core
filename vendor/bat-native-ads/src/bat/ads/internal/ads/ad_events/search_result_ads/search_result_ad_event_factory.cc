/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/ads/ad_events/search_result_ads/search_result_ad_event_factory.h"

#include "base/check.h"
#include "bat/ads/internal/ads/ad_events/search_result_ads/search_result_ad_event_clicked.h"
#include "bat/ads/internal/ads/ad_events/search_result_ads/search_result_ad_event_served.h"
#include "bat/ads/internal/ads/ad_events/search_result_ads/search_result_ad_event_viewed.h"
#include "bat/ads/internal/creatives/search_result_ads/search_result_ad_info.h"

namespace ads::search_result_ads {

std::unique_ptr<AdEventInterface<SearchResultAdInfo>> AdEventFactory::Build(
    const mojom::SearchResultAdEventType event_type) {
  DCHECK(mojom::IsKnownEnumValue(event_type));

  switch (event_type) {
    case mojom::SearchResultAdEventType::kServed: {
      return std::make_unique<AdEventServed>();
    }

    case mojom::SearchResultAdEventType::kViewed: {
      return std::make_unique<AdEventViewed>();
    }

    case mojom::SearchResultAdEventType::kClicked: {
      return std::make_unique<AdEventClicked>();
    }
  }
}

}  // namespace ads::search_result_ads
