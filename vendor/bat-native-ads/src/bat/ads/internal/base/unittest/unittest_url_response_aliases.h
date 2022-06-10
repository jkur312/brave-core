/* Copyright (c) 2022 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_BASE_UNITTEST_UNITTEST_URL_RESPONSE_ALIASES_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_BASE_UNITTEST_UNITTEST_URL_RESPONSE_ALIASES_H_

#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"

namespace ads {

using URLEndpointResponse = std::pair<int, std::string>;
using URLEndpointResponses = std::vector<URLEndpointResponse>;
using URLEndpoints = base::flat_map<std::string, URLEndpointResponses>;

}  // namespace ads

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_BASE_UNITTEST_UNITTEST_URL_RESPONSE_ALIASES_H_