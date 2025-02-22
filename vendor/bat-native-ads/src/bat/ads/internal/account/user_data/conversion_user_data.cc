/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/account/user_data/conversion_user_data.h"

#include "base/check_op.h"
#include "bat/ads/confirmation_type.h"
#include "bat/ads/internal/account/user_data/conversion_user_data_builder.h"

namespace ads::user_data {

void GetConversion(const std::string& creative_instance_id,
                   const ConfirmationType& confirmation_type,
                   const ConversionCallback& callback) {
  DCHECK(!creative_instance_id.empty());
  DCHECK_NE(ConfirmationType::kUndefined, confirmation_type.value());

  if (confirmation_type != ConfirmationType::kConversion) {
    callback(base::Value::Dict());
    return;
  }

  builder::BuildConversion(creative_instance_id,
                           [callback](const base::Value::Dict& user_data) {
                             callback(user_data.Clone());
                           });
}

}  // namespace ads::user_data
