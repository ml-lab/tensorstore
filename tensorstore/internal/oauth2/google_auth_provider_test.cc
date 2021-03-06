// Copyright 2020 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tensorstore/internal/oauth2/google_auth_provider.h"

#include <fstream>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include "absl/time/clock.h"
#include "tensorstore/internal/env.h"
#include "tensorstore/internal/oauth2/fixed_token_auth_provider.h"
#include "tensorstore/internal/oauth2/gce_auth_provider.h"
#include "tensorstore/internal/oauth2/google_service_account_auth_provider.h"
#include "tensorstore/internal/oauth2/oauth2_auth_provider.h"
#include "tensorstore/internal/oauth2/oauth_utils.h"
#include "tensorstore/internal/path.h"
#include "tensorstore/internal/test_util.h"
#include "tensorstore/util/result.h"
#include "tensorstore/util/status.h"
#include "tensorstore/util/to_string.h"

using tensorstore::StrCat;
using tensorstore::internal::JoinPath;
using tensorstore::internal::SetEnv;
using tensorstore::internal::UnsetEnv;
using tensorstore::internal_oauth2::AuthProvider;
using tensorstore::internal_oauth2::GetGoogleAuthProvider;

namespace {

class TestData : public tensorstore::internal::ScopedTemporaryDirectory {
 public:
  std::string WriteApplicationDefaultCredentials() {
    auto p = JoinPath(path(), "application_default_credentials.json");
    std::ofstream ofs(p);
    ofs << R"({
  "client_id": "fake-client-id.apps.googleusercontent.com",
  "client_secret": "fake-client-secret",
  "refresh_token": "fake-refresh-token",
  "type": "authorized_user"
})";
    return p;
  }

  std::string WriteServiceAccountCredentials() {
    auto p = JoinPath(path(), "service_account_credentials.json");
    std::ofstream ofs(p);
    ofs << R"({
  "type": "service_account",
  "project_id": "fake_project_id",
  "private_key_id": "fake_key_id",
  "private_key": "-----BEGIN RSA PRIVATE KEY-----\nMIIEpAIBAAKCAQEAwrEZE6PWQYAy68mWPMuC6KAD02Sb9Pv/FHWpGKe8MxxdDiz/\nspb2KIrWxxZolStHgDXAOoElbAv4GbRLJiivEl8k0gSP9YpIE56nSxfXxRIDH25N\nI3fhRIs5hSG+/p3lLV5NsdNrm1CYHnEbTY7Ow7gpyxl0n+6q+ngguZTOGtBIMqVS\n4KIJlzTlJgeqvLFbtLP6uFc4OuGL6UZ+s4I7zSJVPBRxrFA+mOhBEPz/QjANBHBd\nIEhgh5VlmX/oRUK+D3zR/MnRTYtD8skiZSFMUix1eWvKw/1wX0mieH1rUQbpIYdJ\nTgFhROKuAJWVU7c+T6JHZwm8DqXaVz6oCJPlzwIDAQABAoIBAGHQVAb4A0b5P5wS\ntXZp0KVK72EfZPNaP7dpvcDzVKxhDad3mCeDjLyltG5lpbl7+vpBBwjdpY15Hfbc\nC/1p5ztVrcwOGr2D8d5ZkTc7DV6nRAZghkTRj82+HPH0GF8XuPJoNKSo0aFAhoyU\nyuDWZK8UMXsmmN9ZK3GXNOnIBxyUs703ueIgNkH9zlT2x0wmEs4toZKiPVZhLUrc\nG1zLfuf1onhB5xq7u0sYZCiJrvaVvzNrKune1IrBM+FK/dc3k0vF9NEvwCYxWuTj\nGwO2wU3U945Scj9718pxhMMxZpsPZfMZHrYcdMvjpPaKFhJjxb16kT4gvSdm015j\nLgpM1xECgYEA35/KW4npUPoltBZ2Gi/YPmGVfpyXz6ToOw9ENawiGdNrOQG1Pw+v\nPBV0+yvcp1AvlL46lp87xQrl0dYHwwsQ7eRqpeyG6PCXRN7pJXP9Dac6Tq07lu2g\nriltHcuw8WYLv0gjrNr8IaCN04VS30d8MayXgHuvR3+NHkBdryuKFgsCgYEA3uD7\nmNukdNxJBQhgOO8lCbLXdEjgFFDBuh/9GvpqaeILP4MIwpWj9tA9Hjw5JlK3qpHL\nvLsJinKMmaswX43Hzf8OAAhTkSC/TfIJwZTGuBPoDH4UnMD+83SAk8DDgWTUvz/6\n1ilR4zm3kus6ZxTA1zp3P5UFD2etbv+cmGkjHc0CgYBkpw1z6j0j/5Oc3UdHPiW8\n3jtlg6IpCfalLpfq+JFYwnpObGBiA/NBvf6rVvC4NjVUY9MHHKDQbblHm2he98ok\n6Vy/VhjbG/9aNmMGQpCx5oUuCHb71fUuruK4OIhp/x5meFfmY6J8mEF95VKJwSk7\nSo3efM1GBzlDVoFUaOp8RQKBgQDWBQ0Ul7WwUef8YTKk+V+DlKy4CVLDr1iYNieC\nRHzy+BD9CALdd3xfgU9vPT1Tw5KCxEX0EVb0D1NcLLrixu7arNTwyw4UCnIpkwYz\nUX4RPWxSsq9wZxNrDLB7MVuLYRu6GuHvzPXJUJ8rAZ6vZYpYIthnwd1+EXzFXcct\nw6fo8QKBgQClY0EmhGIoDHNPjPOGzl2hmZCm5FKPx9i2SOOVYuSMdPT3qTYOp4/Q\nUp1oqkbd1ZWxMlbuRljpwbUHRcj85O5bkmWylINjpA1hFqxcxtj1r9xRmeO9Qcqa\n89jOblkbSoVDE5CFHD0Cv4bFw09z/l6Ih9DOW4AlB5UN+byEUPsIdw==\n-----END RSA PRIVATE KEY-----",
  "client_email": "fake-test-project.iam.gserviceaccount.com",
  "client_id": "fake_client_id",
  "auth_uri": "https://accounts.google.com/o/oauth2/auth",
  "token_uri": "https://accounts.google.com/o/oauth2/token",
  "auth_provider_x509_cert_url": "https://www.googleapis.com/oauth2/v1/certs",
  "client_x509_cert_url": "https://www.googleapis.com/robot/v1/metadata/x509/fake-test-project.iam.gserviceaccount.com"
})";
    return p;
  }
};

class GoogleAuthProviderTest : public ::testing::Test {
  void SetUp() override { ClearEnvVars(); }

  void TearDown() override { ClearEnvVars(); }

  void ClearEnvVars() {
    UnsetEnv("GOOGLE_APPLICATION_CREDENTIALS");
    UnsetEnv("CLOUDSDK_CONFIG");
    UnsetEnv("GOOGLE_AUTH_TOKEN_FOR_TESTING");
  }
};

TEST_F(GoogleAuthProviderTest, Invalid) {
  // All environment variables are unset by default; this will look for
  // GCE, which will fail, and will return an error status.
  //
  // Set GCE_METADATA_ROOT to dummy value to ensure GCE detection fails even if
  // the test is really being run on GCE.
  SetEnv("GCE_METADATA_ROOT", "invalidmetadata.google.internal");
  auto auth_provider = GetGoogleAuthProvider();
  EXPECT_FALSE(auth_provider.ok());
  UnsetEnv("GCE_METADATA_ROOT");
}

TEST_F(GoogleAuthProviderTest, AuthTokenForTesting) {
  SetEnv("GOOGLE_AUTH_TOKEN_FOR_TESTING", "abc");

  // GOOGLE_AUTH_TOKEN_FOR_TESTING, so a FixedTokenAuthProvider
  // with the provided token will be returned.
  auto auth_provider = GetGoogleAuthProvider();
  ASSERT_TRUE(auth_provider.ok()) << auth_provider.status();

  // Expect an instance of FixedTokenAuthProvider.
  {
    auto instance =
        dynamic_cast<tensorstore::internal_oauth2::FixedTokenAuthProvider*>(
            auth_provider->get());
    EXPECT_FALSE(instance == nullptr);
  }

  // The token value is the same as was set by setenv()
  std::unique_ptr<AuthProvider> auth = std::move(*auth_provider);
  auto token = auth->GetToken();
  ASSERT_TRUE(token.ok());
  EXPECT_EQ("abc", token->token);
}

TEST_F(GoogleAuthProviderTest, GoogleOAuth2AccountCredentialsFromSDKConfig) {
  TestData test_data;
  test_data.WriteServiceAccountCredentials();
  test_data.WriteApplicationDefaultCredentials();
  SetEnv("CLOUDSDK_CONFIG", test_data.path().c_str());

  // CLOUDSDK_CONFIG has been set to the path of the credentials file.
  // We will attempt to parse the "application_default_credentials.json"
  // file in that location, which happens to be an OAuth2 token.
  auto auth_provider = GetGoogleAuthProvider();
  ASSERT_TRUE(auth_provider.ok()) << auth_provider.status();

  // Expect an instance of OAuth2AuthProvider
  {
    auto instance =
        dynamic_cast<tensorstore::internal_oauth2::OAuth2AuthProvider*>(
            auth_provider->get());
    EXPECT_FALSE(instance == nullptr);
  }
}

/// GOOGLE_APPLICATION_CREDENTIALS
TEST_F(GoogleAuthProviderTest, GoogleOAuth2AccountCredentials) {
  TestData test_data;
  SetEnv("GOOGLE_APPLICATION_CREDENTIALS",
         test_data.WriteApplicationDefaultCredentials().c_str());

  // GOOGLE_APPLICATION_CREDENTIALS has been set to the path of the
  // application_default_credentials.json file, which is an OAuth2 token.
  auto auth_provider = GetGoogleAuthProvider();
  ASSERT_TRUE(auth_provider.ok()) << auth_provider.status();

  // Expect an instance of OAuth2AuthProvider
  {
    auto instance =
        dynamic_cast<tensorstore::internal_oauth2::OAuth2AuthProvider*>(
            auth_provider->get());
    EXPECT_FALSE(instance == nullptr);
  }
}

TEST_F(GoogleAuthProviderTest, GoogleServiceAccountCredentials) {
  TestData test_data;
  SetEnv("GOOGLE_APPLICATION_CREDENTIALS",
         test_data.WriteServiceAccountCredentials().c_str());

  // GOOGLE_APPLICATION_CREDENTIALS has been set to the path of the
  // service_account_credentials.json file, which is an Google Service Account
  // credentials token.
  auto auth_provider = GetGoogleAuthProvider();
  ASSERT_TRUE(auth_provider.ok()) << auth_provider.status();

  // Expect an instance of GoogleServiceAccountAuthProvider
  {
    auto instance = dynamic_cast<
        tensorstore::internal_oauth2::GoogleServiceAccountAuthProvider*>(
        auth_provider->get());
    EXPECT_FALSE(instance == nullptr);
  }
}

// NOTE: ${HOME}/.cloud/config/application_default_credentials.json is not
// tested.
//
// NOTE: GCE metadata credentials testing would require mocking the GCE
// metadata service, which we have not done.

}  // namespace
