/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/token.h"
#include "base/values.h"
#include "brave/browser/playlist/playlist_service_factory.h"
#include "brave/components/playlist/features.h"
#include "brave/components/playlist/playlist_constants.h"
#include "brave/components/playlist/playlist_service.h"
#include "brave/components/playlist/playlist_service_helper.h"
#include "brave/components/playlist/playlist_service_observer.h"
#include "brave/components/playlist/playlist_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace playlist {

namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  if (request.relative_url == "/valid_thumbnail" ||
      request.relative_url == "/valid_media_file_1" ||
      request.relative_url == "/valid_media_file_2") {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("image/gif");
    http_response->set_content("thumbnail");
  } else {
    http_response->set_code(net::HTTP_NOT_FOUND);
  }

  return std::move(http_response);
}

}  // namespace

class PlaylistBrowserTest : public InProcessBrowserTest,
                            public PlaylistServiceObserver {
 public:
  PlaylistBrowserTest() : weak_factory_(this) {
    scoped_feature_list_.InitAndEnableFeature(playlist::features::kPlaylist);
  }
  ~PlaylistBrowserTest() override {}

  // InProcessBrowserTest overrides:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");

    // Set up embedded test server to handle fake responses.
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    https_server_->RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(https_server_->Start());

    GetPlaylistService()->AddObserver(this);
    ResetStatus();
  }

  void TearDownOnMainThread() override {
    GetPlaylistService()->RemoveObserver(this);
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    mock_cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
    InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  // PlaylistServiceObserver overrides:
  void OnPlaylistItemStatusChanged(
      const PlaylistItemChangeParams& params) override {
    VLOG(2) << __func__
            << PlaylistItemChangeParams::GetPlaylistChangeTypeAsString(
                   params.change_type);
    on_playlist_changed_called_count_++;
    change_params_ = params;
    called_change_types_.insert(change_params_.change_type);

    if (change_params_.change_type == PlaylistItemChangeParams::Type::kAdded) {
      lastly_added_playlist_id_ = change_params_.playlist_id;
    }

    if (on_playlist_changed_called_count_ ==
            on_playlist_changed_called_target_count_ ||
        change_params_.change_type ==
            PlaylistItemChangeParams::Type::kAborted) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(&base::RunLoop::Quit, base::Unretained(run_loop())));
    }
  }

  PlaylistService* GetPlaylistService() {
    return PlaylistServiceFactory::GetInstance()->GetForBrowserContext(
        browser()->profile());
  }

  void ResetStatus() {
    on_playlist_changed_called_count_ = 0;
    on_playlist_changed_called_target_count_ = 0;
    called_change_types_.clear();
  }

  void WaitForEvents(int n) {
    on_playlist_changed_called_target_count_ = n;

    if (on_playlist_changed_called_count_ <
        on_playlist_changed_called_target_count_)
      Run();
  }

  void Run() {
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop()->Run();
  }

  PlaylistItemInfo GetValidCreateParams() {
    PlaylistItemInfo params;
    params.id = base::Token::CreateRandom().ToString();
    params.title = "Valid playlist creation params";
    params.thumbnail_path =
        https_server()->GetURL("thumbnail.com", "/valid_thumbnail").spec();
    params.media_file_path =
        https_server()->GetURL("song.com", "/valid_media_file_1").spec();
    return params;
  }

  PlaylistItemInfo GetValidCreateParamsForIncompleteMediaFileList() {
    PlaylistItemInfo params;
    params.id = base::Token::CreateRandom().ToString();
    params.title = "Valid playlist creation params";
    params.thumbnail_path =
        https_server()->GetURL("thumbnail.com", "/valid_thumbnail").spec();
    params.media_file_path =
        https_server()
            ->GetURL("not_existing_song.com", "/invalid_media_file")
            .spec();
    return params;
  }

  PlaylistItemInfo GetInvalidCreateParams() {
    PlaylistItemInfo params;
    params.id = base::Token::CreateRandom().ToString();
    params.title = "Valid playlist creation params";
    params.thumbnail_path =
        https_server()
            ->GetURL("not_existing_thumbnail.com", "/invalid_thumbnail")
            .spec();
    params.media_file_path =
        https_server()
            ->GetURL("not_existing_song.com", "/invalid_media_file")
            .spec();
    return params;
  }

  void CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type type) {
    if (called_change_types_.find(type) != called_change_types_.end())
      return;

    std::string log =
        "type" + PlaylistItemChangeParams::GetPlaylistChangeTypeAsString(type) +
        " wasn't found: [";
    for (const auto& type : called_change_types_) {
      log +=
          PlaylistItemChangeParams::GetPlaylistChangeTypeAsString(type) + ", ";
    }
    log += "]";
    FAIL() << log;
  }

  void OnDeleteAllPlaylist(bool deleted) { EXPECT_TRUE(deleted); }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  base::RunLoop* run_loop() const { return run_loop_.get(); }

  content::ContentMockCertVerifier mock_cert_verifier_;

  int on_playlist_changed_called_count_ = 0;
  int on_playlist_changed_called_target_count_ = 0;
  std::string lastly_added_playlist_id_;

  base::flat_set<PlaylistItemChangeParams::Type> called_change_types_;

  PlaylistItemChangeParams change_params_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::WeakPtrFactory<PlaylistBrowserTest> weak_factory_;
};

IN_PROC_BROWSER_TEST_F(PlaylistBrowserTest, CreatePlaylist) {
  auto* service = GetPlaylistService();

  // When a playlist is created and all goes well, we will receive 3
  // notifications: added, thumbnail ready and play ready.
  service->CreatePlaylistItem(GetValidCreateParams());
  WaitForEvents(3);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kAdded);
  CheckIsPlaylistChangeTypeCalled(
      PlaylistItemChangeParams::Type::kThumbnailReady);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kPlayReady);
}

IN_PROC_BROWSER_TEST_F(PlaylistBrowserTest, ThumbnailFailed) {
  auto* service = GetPlaylistService();

  // When a playlist is created and the thumbnail can not be downloaded, we will
  // receive 3 notifications: added, thumbnail failed and ready.
  auto param = GetInvalidCreateParams();
  param.media_file_path = GetValidCreateParams().media_file_path;
  service->CreatePlaylistItem(param);
  WaitForEvents(3);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kAdded);
  CheckIsPlaylistChangeTypeCalled(
      PlaylistItemChangeParams::Type::kThumbnailFailed);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kPlayReady);
}

IN_PROC_BROWSER_TEST_F(PlaylistBrowserTest, MediaDownloadFailed) {
  auto* service = GetPlaylistService();

  // When a playlist is created and media file source is invalid,
  // we will receive 2 notifications: added and aborted.
  // Thumbnail downloading can be canceled.
  service->CreatePlaylistItem(GetValidCreateParamsForIncompleteMediaFileList());
  WaitForEvents(3);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kAdded);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kAborted);
}

IN_PROC_BROWSER_TEST_F(PlaylistBrowserTest, ApiFunctions) {
  auto* service = GetPlaylistService();

  VLOG(2) << "create playlist 1";
  ResetStatus();
  service->CreatePlaylistItem(GetValidCreateParams());
  WaitForEvents(3);

  VLOG(2) << "create playlist 2";
  ResetStatus();
  service->CreatePlaylistItem(GetValidCreateParams());
  WaitForEvents(3);

  VLOG(2) << "create playlist 3 but should fail";
  ResetStatus();
  service->CreatePlaylistItem(GetValidCreateParamsForIncompleteMediaFileList());
  WaitForEvents(3);

  ResetStatus();
  base::Value items = service->GetAllPlaylistItems();
  EXPECT_EQ(3UL, items.GetList().size());

  ResetStatus();
  base::Value item = service->GetPlaylistItem(lastly_added_playlist_id_);
  auto* id = item.FindStringKey(kPlaylistIDKey);
  EXPECT_TRUE(id);
  EXPECT_EQ(lastly_added_playlist_id_.compare(*id), 0);

  VLOG(2) << "recover item but should fail";
  // When we try to recover with same playlist item, we should get
  // notification: kAborted because included media files are still
  // invalid_media_file. before we get kAborted message, we may get
  // kThunbmailReady.
  ResetStatus();
  service->RecoverPlaylistItem(lastly_added_playlist_id_);
  WaitForEvents(2);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kAborted);

  // To simulate invalid media file url becomes valid, change media file url.
  // With this, recovery process will get 1 kPlayReady notification.
  ResetStatus();

  VLOG(2) << "recover item and should succeed";
  item.SetPath(
      kPlaylistItemMediaFilePathKey,
      base::Value(
          https_server()->GetURL("song.com", "/valid_media_file_1").spec()));
  const auto* thumbnail_path =
      item.FindStringPath(kPlaylistItemThumbnailPathKey);
  DCHECK(thumbnail_path);
  GURL thumbnail_url(*thumbnail_path);

  service->UpdatePlaylistItemValue(lastly_added_playlist_id_, std::move(item));
  service->RecoverPlaylistItem(lastly_added_playlist_id_);

  if (thumbnail_url.SchemeIsFile() || !thumbnail_url.is_valid()) {
    WaitForEvents(1);
  } else {
    WaitForEvents(2);
    CheckIsPlaylistChangeTypeCalled(
        PlaylistItemChangeParams::Type::kThumbnailReady);
  }

  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kPlayReady);

  VLOG(2) << "delete item";
  // When a playlist is deleted, we should get 1 notification: deleted.
  ResetStatus();
  service->DeletePlaylistItem(lastly_added_playlist_id_);
  EXPECT_EQ(1, on_playlist_changed_called_count_);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kDeleted);

  // After deleting one playlist, total playlist count should be 2.
  ResetStatus();
  items = service->GetAllPlaylistItems();
  EXPECT_EQ(2UL, items.GetList().size());

  VLOG(2) << "delete all items";
  // When all playlist are deleted, we should get 1 notification: all deleted.
  ResetStatus();
  service->DeleteAllPlaylistItems();
  EXPECT_EQ(1, on_playlist_changed_called_count_);
  CheckIsPlaylistChangeTypeCalled(PlaylistItemChangeParams::Type::kAllDeleted);

  // After deleting all playlist, total playlist count should be 0.
  ResetStatus();
  items = service->GetAllPlaylistItems();
  EXPECT_EQ(0UL, items.GetList().size());
}

}  // namespace playlist