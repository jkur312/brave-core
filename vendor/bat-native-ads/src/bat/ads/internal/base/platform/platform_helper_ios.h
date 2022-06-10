/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_BASE_PLATFORM_PLATFORM_HELPER_IOS_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_BASE_PLATFORM_PLATFORM_HELPER_IOS_H_

#include <string>

#include "base/no_destructor.h"
#include "bat/ads/internal/base/platform/platform_helper.h"

namespace ads {

class PlatformHelperIos final : public PlatformHelper {
 public:
  ~PlatformHelperIos() override;

  PlatformHelperIos(const PlatformHelperIos&) = delete;
  PlatformHelperIos& operator=(const PlatformHelperIos&) = delete;

 protected:
  friend class base::NoDestructor<PlatformHelperIos>;

  PlatformHelperIos();

 private:
  // PlatformHelper impl
  bool IsMobile() const override;
  std::string GetName() const override;
  PlatformType GetType() const override;
};

}  // namespace ads

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_BASE_PLATFORM_PLATFORM_HELPER_IOS_H_