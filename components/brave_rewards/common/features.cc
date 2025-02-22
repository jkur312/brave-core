/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_rewards/common/features.h"

#include "base/feature_list.h"

namespace brave_rewards {
namespace features {

#if BUILDFLAG(IS_ANDROID)
//  Flag for Brave Rewards.
#if defined(ARCH_CPU_X86_FAMILY) && defined(OFFICIAL_BUILD)
BASE_FEATURE(kBraveRewards, "BraveRewards", base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kBraveRewards, "BraveRewards", base::FEATURE_ENABLED_BY_DEFAULT);
#endif
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_GEMINI_WALLET)
BASE_FEATURE(kGeminiFeature,
             "BraveRewardsGemini",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kVerboseLoggingFeature,
             "BraveRewardsVerboseLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace brave_rewards
