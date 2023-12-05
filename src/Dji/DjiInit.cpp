#include "DjiInit.h"

#include "openssl/rsa.h"

#include "Common/config.h"

#include <iostream>

namespace mediakit {

const char* kPathPublickKey = "/tmp/pub_key";
const char* kPathPrivateKey = "/tmp/private_key";
const char* kPathAppLicense = "/tmp/app_license";

KeyStoreDefault::KeyStoreDefault() 
{
    memset(buffer, 0, sizeof(buffer));

    if (ReadKeys()) {
        return;
    }
    auto ret = GenerateKeys();
    ReadKeys();
    if (!ret) printf("ERROR: can not get valid keys\n");
}

edge_sdk::ErrorCode KeyStoreDefault::RSA2048_GetDERPrivateKey(std::string& private_key) const 
{
    if (rsa2048_private_key_ == nullptr) {
        return edge_sdk::kErrorParamGetFailure;
    }
    private_key = *rsa2048_private_key_;
    return edge_sdk::kOk;
}

edge_sdk::ErrorCode KeyStoreDefault::RSA2048_GetDERPublicKey(std::string& public_key) const 
{
    if (rsa2048_public_key_== nullptr) {
        return edge_sdk::kErrorParamGetFailure;
    }
    public_key = *rsa2048_public_key_;
    return edge_sdk::kOk;
}

bool KeyStoreDefault::ReadKeys() 
{
    uint32_t len = 0;
    FILE* file = fopen(kPathPublickKey, "rb");
    if (file) {
        auto ret = fread(buffer, 1, sizeof(buffer), file);
        if (ret > 0) {
            len = ret;
        }
        InfoL << "public key len read len:" << len;
        fclose(file);
    } else {
        return false;
    }
    rsa2048_public_key_ = std::make_shared<std::string>((char*)buffer, len);

    memset(buffer, 0, sizeof(buffer));

    file = fopen(kPathPrivateKey, "rb");
    if (file) {
        auto ret = fread(buffer, 1, sizeof(buffer), file);
        if (ret > 0) {
            len = ret;
        }
        InfoL << "private key len read len:" << len;
        fclose(file);
    } else {
        return false;
    }
    rsa2048_private_key_ = std::make_shared<std::string>((char*)buffer, len);
    return true;
}

bool KeyStoreDefault::GenerateKeys() 
{
    RSA* rsa2048 = RSA_generate_key(2048, RSA_F4, NULL, NULL);
    uint8_t* PKey = buffer;
    auto key_len = i2d_RSAPublicKey(rsa2048, &PKey);

    if (!rsa2048) {
        return false;
    }
    rsa2048_public_key_ = std::make_shared<std::string>((char*)buffer, key_len);

    FILE* pkey = fopen(kPathPublickKey, "wb");
    if (pkey) {
        fwrite(buffer, 1, key_len, pkey);
        fclose(pkey);
    }

    uint8_t* PrivKey = buffer;
    key_len = i2d_RSAPrivateKey(rsa2048, &PrivKey);
    InfoL << "Private key len=", key_len;
    rsa2048_private_key_ = std::make_shared<std::string>((char*)buffer, key_len);

    FILE* privkey = fopen(kPathPrivateKey, "wb");
    if (privkey) {
        fwrite(buffer, 1, key_len, privkey);
        fclose(privkey);
    }
    RSA_free(rsa2048);
    return true;
}

static edge_sdk::ErrorCode PrintConsoleFunc(const uint8_t* data, uint16_t dataLen) {
    InfoL << "DJI | " << data;

    return edge_sdk::kOk;
}

static edge_sdk::ErrorCode InitOptions(edge_sdk::Options& option) {
    option.product_name = "Edge-1.0";
    option.vendor_name = "Vendor";
    option.serial_number = "SN0000100010101";
    option.firmware_version = {0, 1, 0, 0};

    GET_CONFIG(std::string, appName, Dji::kAppName);
	GET_CONFIG(std::string, appId, Dji::kAppId);
    GET_CONFIG(std::string, appKey, Dji::kAppKey);
    GET_CONFIG(std::string, appLicense, Dji::kAppLicense);
	GET_CONFIG(std::string, developerAccount, Dji::kDeveloperAccount);

    // read app license
    const char* licenseParh = kPathAppLicense;
    if (!appLicense.empty()) {
        licenseParh = appLicense.c_str();
    }

    char license_buffer[1500] = {0};
    uint32_t len = 0;
    FILE* file = fopen(licenseParh, "rb");
    if (file) {
        auto ret = fread(license_buffer, 1, sizeof(license_buffer), file);
        if (ret > 0) {
            len = ret;
        }
        InfoL << "app license len read len:" << len;
        fclose(file);
    } else {
        ErrorL << "read app license file failed, kpath:" << licenseParh;
    }

    edge_sdk::AppInfo app_info;
    app_info.app_name = appName;
    app_info.app_id = appId;
    app_info.app_key = appKey;
    app_info.app_license = std::string(license_buffer, len);

    option.app_info = app_info;
    edge_sdk::LoggerConsole console = {edge_sdk::kLevelDebug, PrintConsoleFunc, true};
    option.logger_console_lists.push_back(console);

    option.key_store = std::make_shared<KeyStoreDefault>();

    return edge_sdk::kOk;
}

edge_sdk::ErrorCode ESDKInit() 
{
    edge_sdk::Options option;
    InitOptions(option);
    auto rc = edge_sdk::ESDKInit::Instance()->Init(option);
    if (rc != edge_sdk::kOk) {
        std::cout << "Edge sdk init failed: " << rc << std::endl;
    }
    return rc;
}

edge_sdk::ErrorCode ESDKDeInit() 
{ 
    return edge_sdk::ESDKInit::Instance()->DeInit(); 
}

}