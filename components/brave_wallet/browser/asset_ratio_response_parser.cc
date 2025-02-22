/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/brave_wallet/browser/asset_ratio_response_parser.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "brave/components/brave_wallet/common/brave_wallet_types.h"
#include "brave/components/brave_wallet/common/eth_address.h"
#include "brave/components/brave_wallet/common/hex_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace brave_wallet {

absl::optional<std::string> ParseSardineAuthToken(const std::string& json) {
  // Parses results like this:
  // {
  //   "clientToken":"74618e17-a537-4f5d-ab4d-9916739560b1",
  //   "expiresAt":"2022-07-25T19:59:57Z"
  //   "name": "brave-core",
  // }

  absl::optional<base::Value> records_v =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                       base::JSONParserOptions::JSON_PARSE_RFC);
  if (!records_v || !records_v->is_dict()) {
    VLOG(0) << "Invalid response, could not parse JSON, JSON is: " << json;
    return absl::nullopt;
  }

  const std::string* auth_token =
      records_v->GetDict().FindString("clientToken");
  if (!auth_token) {
    return absl::nullopt;
  }

  return *auth_token;
}

bool ParseAssetPrice(const std::string& json,
                     const std::vector<std::string>& from_assets,
                     const std::vector<std::string>& to_assets,
                     std::vector<mojom::AssetPricePtr>* values) {
  // Parses results like this:
  // /v2/relative/provider/coingecko/bat,chainlink/btc,usd/1w
  // {
  //  "payload": {
  //    "chainlink": {
  //      "btc": 0.00063075,
  //      "usd": 29.17,
  //      "btc_timeframe_change": -0.9999742658279261,
  //      "usd_timeframe_change": 0.1901162098990581
  //    },
  //    "bat": {
  //      "btc": 1.715e-05,
  //      "usd": 0.793188,
  //      "btc_timeframe_change": -0.9999993002916352,
  //      "usd_timeframe_change": -0.9676384677306338
  //    }
  //  },
  //  "lastUpdated": "2021-08-16T15:45:11.901Z"
  // }

  DCHECK(values);

  absl::optional<base::Value> records_v =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                       base::JSONParserOptions::JSON_PARSE_RFC);
  if (!records_v || !records_v->is_dict()) {
    LOG(ERROR) << "Invalid response, could not parse JSON, JSON is: " << json;
    return false;
  }

  const auto& response_dict = records_v->GetDict();
  const auto* payload = response_dict.FindDict("payload");
  if (!payload) {
    return false;
  }

  for (const std::string& from_asset : from_assets) {
    const auto* from_asset_dict = payload->FindDictByDottedPath(from_asset);
    if (!from_asset_dict) {
      return false;
    }

    for (const std::string& to_asset : to_assets) {
      auto asset_price = mojom::AssetPrice::New();
      asset_price->from_asset = from_asset;
      asset_price->to_asset = to_asset;

      absl::optional<double> to_price =
          from_asset_dict->FindDoubleByDottedPath(to_asset);
      if (!to_price) {
        return false;
      }
      asset_price->price = base::NumberToString(*to_price);
      std::string to_asset_timeframe_key =
          base::StringPrintf("%s_timeframe_change", to_asset.c_str());
      absl::optional<double> to_timeframe_change =
          from_asset_dict->FindDoubleByDottedPath(to_asset_timeframe_key);
      if (!to_timeframe_change) {
        return false;
      }
      asset_price->asset_timeframe_change =
          base::NumberToString(*to_timeframe_change);

      values->push_back(std::move(asset_price));
    }
  }

  return true;
}

bool ParseAssetPriceHistory(const std::string& json,
                            std::vector<mojom::AssetTimePricePtr>* values) {
  DCHECK(values);

  // {  "payload":
  //   {
  //     "prices":[[1622733088498,0.8201346624954003],[1622737203757,0.8096978545029869]],
  //     "market_caps":[[1622733088498,1223507820.383275],[1622737203757,1210972881.4928021]],
  //     "total_volumes":[[1622733088498,163426828.00299588],[1622737203757,157618689.0971025]]
  //   }
  // }

  absl::optional<base::Value> records_v =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                       base::JSONParserOptions::JSON_PARSE_RFC);
  if (!records_v || !records_v->is_dict()) {
    LOG(ERROR) << "Invalid response, could not parse JSON, JSON is: " << json;
    return false;
  }

  const auto& response_dict = records_v->GetDict();
  const auto* payload = response_dict.FindDict("payload");
  if (!payload) {
    return false;
  }

  const auto* prices_list = payload->FindList("prices");
  if (!prices_list) {
    return false;
  }

  for (const auto& date_price_list_it : *prices_list) {
    const auto* date_price_list = date_price_list_it.GetIfList();
    if (!date_price_list) {
      return false;
    }
    auto it = date_price_list->begin();
    const auto& date_value = *it;
    const auto& price_value = *(++it);

    // Check whether date_value is convertible to a double first.
    if (!date_value.is_double() && !date_value.is_int())
      return false;
    double date_dbl = date_value.GetDouble();

    // Check whether price_value is convertible to a double first.
    if (!price_value.is_double() && !price_value.is_int())
      return false;
    double price = price_value.GetDouble();

    base::Time date = base::Time::FromJsTime(date_dbl);
    auto asset_time_price = mojom::AssetTimePrice::New();
    asset_time_price->date = base::Milliseconds(date.ToJavaTime());
    asset_time_price->price = base::NumberToString(price);
    values->push_back(std::move(asset_time_price));
  }

  return true;
}

std::string ParseEstimatedTime(const std::string& json) {
  // {
  //   "payload": {
  //     "status": "1",
  //     "message": "",
  //     "result": "3615"
  //   },
  //   "lastUpdated": "2021-09-22T21:45:40.015Z"
  // }

  absl::optional<base::Value> records_v =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                       base::JSONParserOptions::JSON_PARSE_RFC);
  if (!records_v || !records_v->is_dict()) {
    LOG(ERROR) << "Invalid response, could not parse JSON, JSON is: " << json;
    return "";
  }

  const std::string* result =
      records_v->GetDict().FindStringByDottedPath("payload.result");
  return result ? *result : "";
}

mojom::BlockchainTokenPtr ParseTokenInfo(const std::string& json,
                                         const std::string& chain_id,
                                         mojom::CoinType coin) {
  // {
  //   "payload": {
  //     "status": "1",
  //     "message": "OK",
  //     "result": [
  //       {
  //         "contractAddress": "0xdac17f958d2ee523a2206206994597c13d831ec7",
  //         "tokenName": "Tether USD",
  //         "symbol": "USDT",
  //         "divisor": "6",
  //         "tokenType": "ERC20",
  //         "totalSupply": "39828710009874796",
  //         "blueCheckmark": "true",
  //         "description": "Tether gives you the joint benefits of open...",
  //         "website": "https://tether.to/",
  //         "email": "support@tether.to",
  //         "blog": "https://tether.to/category/announcements/",
  //         "reddit": "",
  //         "slack": "",
  //         "facebook": "",
  //         "twitter": "https://twitter.com/Tether_to",
  //         "bitcointalk": "",
  //         "github": "",
  //         "telegram": "",
  //         "wechat": "",
  //         "linkedin": "",
  //         "discord": "",
  //         "whitepaper": "https://path/to/TetherWhitePaper.pdf",
  //         "tokenPriceUSD": "1.000000000000000000"
  //       }
  //     ]
  //   },
  //   "lastUpdated": "2021-12-09T22:02:23.187Z"
  // }

  absl::optional<base::Value> records_v =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                       base::JSONParserOptions::JSON_PARSE_RFC);
  if (!records_v || !records_v->is_dict()) {
    LOG(ERROR) << "Invalid response, could not parse JSON, JSON is: " << json;
    return nullptr;
  }

  const auto& response_dict = records_v->GetDict();
  const auto* result = response_dict.FindListByDottedPath("payload.result");
  if (!result)
    return nullptr;

  if (result->size() != 1)
    return nullptr;
  const auto* token = (*result)[0].GetIfDict();
  if (!token)
    return nullptr;

  const std::string* contract_address = token->FindString("contractAddress");
  if (!contract_address)
    return nullptr;
  const auto eth_addr = EthAddress::FromHex(*contract_address);
  if (eth_addr.IsEmpty())
    return nullptr;

  const std::string* name = token->FindString("tokenName");
  if (!name || name->empty())
    return nullptr;

  const std::string* symbol = token->FindString("symbol");
  if (!symbol || symbol->empty())
    return nullptr;

  const std::string* decimals_string = token->FindString("divisor");
  int decimals = 0;
  if (!decimals_string || !base::StringToInt(*decimals_string, &decimals))
    return nullptr;

  const std::string* token_type = token->FindString("tokenType");
  if (!token_type)
    return nullptr;

  bool is_erc20 = base::EqualsCaseInsensitiveASCII(*token_type, "ERC20");
  bool is_erc721 = base::EqualsCaseInsensitiveASCII(*token_type, "ERC721");
  if (!is_erc20 && !is_erc721)  // unsupported token
    return nullptr;

  return mojom::BlockchainToken::New(
      eth_addr.ToChecksumAddress(), *name, "" /* logo */, is_erc20, is_erc721,
      is_erc721 /* is_nft */, *symbol, decimals, true, "", "", chain_id, coin);
}

bool ParseCoinMarkets(const std::string& json,
                      std::vector<mojom::CoinMarketPtr>* values) {
  DCHECK(values);
  // {
  //   "payload": [
  //     {
  //       "id": "bitcoin",
  //       "symbol": "btc",
  //       "name": "Bitcoin",
  //       "image":
  //       "https://assets.coingecko.com/coins/images/1/large/bitcoin.png?1547033579",
  //       "market_cap": 727960800075,
  //       "market_cap_rank": 1,
  //       "current_price": 38357,
  //       "price_change_24h": -1229.64683216549,
  //       "price_change_percentage_24h": -3.10625,
  //       "total_volume": 17160995925
  //     },
  //     {
  //       "id": "ethereum",
  //       "symbol": "eth",
  //       "name": "Ethereum",
  //       "image":
  //       "https://assets.coingecko.com/coins/images/279/large/ethereum.png?1595348880",
  //       "market_cap": 304535808667,
  //       "market_cap_rank": 2,
  //       "current_price": 2539.82,
  //       "price_change_24h": -136.841895278459,
  //       "price_change_percentage_24h": -5.11242,
  //       "total_volume": 9583014937
  //     }
  //   ],
  //   "lastUpdated": "2022-03-07T00:25:12.259823452Z"
  // }
  absl::optional<base::Value> records_v =
      base::JSONReader::Read(json, base::JSON_PARSE_CHROMIUM_EXTENSIONS |
                                       base::JSONParserOptions::JSON_PARSE_RFC);
  if (!records_v) {
    VLOG(0) << "Invalid response, could not parse JSON, JSON is: " << json;
    return false;
  }

  if (!records_v->is_dict()) {
    return false;
  }

  auto* payload = records_v->FindListKey("payload");
  if (!payload) {
    return false;
  }

  for (const auto& coin_market_list_it : payload->GetList()) {
    if (!coin_market_list_it.is_dict()) {
      return false;
    }
    auto coin_market = mojom::CoinMarket::New();
    auto* id = coin_market_list_it.FindStringKey("id");
    if (!id) {
      return false;
    }
    coin_market->id = *id;

    auto* symbol = coin_market_list_it.FindStringKey("symbol");
    if (!symbol) {
      return false;
    }
    coin_market->symbol = *symbol;

    auto* name = coin_market_list_it.FindStringKey("name");
    if (!name) {
      return false;
    }
    coin_market->name = *name;

    auto* image = coin_market_list_it.FindStringKey("image");
    if (!image) {
      return false;
    }
    coin_market->image = *image;

    absl::optional<double> market_cap =
        coin_market_list_it.FindDoubleKey("market_cap");
    if (!market_cap) {
      return false;
    }
    coin_market->market_cap = *market_cap;

    absl::optional<uint32_t> market_cap_rank =
        coin_market_list_it.FindIntKey("market_cap_rank");
    if (!market_cap_rank) {
      return false;
    }
    coin_market->market_cap_rank = *market_cap_rank;

    absl::optional<double> current_price =
        coin_market_list_it.FindDoubleKey("current_price");
    if (!current_price) {
      return false;
    }
    coin_market->current_price = *current_price;

    absl::optional<double> price_change_24h =
        coin_market_list_it.FindDoubleKey("price_change_24h");
    if (!price_change_24h) {
      return false;
    }
    coin_market->price_change_24h = *price_change_24h;

    absl::optional<double> price_change_percentage_24h =
        coin_market_list_it.FindDoubleKey("price_change_percentage_24h");
    if (!price_change_percentage_24h) {
      return false;
    }
    coin_market->price_change_percentage_24h = *price_change_percentage_24h;

    absl::optional<double> total_volume =
        coin_market_list_it.FindDoubleKey("total_volume");
    if (!total_volume) {
      return false;
    }
    coin_market->total_volume = *total_volume;

    values->push_back(std::move(coin_market));
  }

  return true;
}

}  // namespace brave_wallet
