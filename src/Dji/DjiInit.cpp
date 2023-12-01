#include "DjiInit.h"

#include "openssl/rsa.h"

#include <iostream>

namespace mediakit {

const char* kPathPublickKey = "/tmp/pub_key";
const char* kPathPrivateKey = "/tmp/private_key";

KeyStoreDefault::KeyStoreDefault() 
{
    memset(buffer, 0, 1500);

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
        printf("public key read len: %ld\n", ret);
        fclose(file);
    } else {
        return false;
    }
    rsa2048_public_key_ = std::make_shared<std::string>((char*)buffer, len);

    file = fopen(kPathPrivateKey, "rb");
    if (file) {
        auto ret = fread(buffer, 1, sizeof(buffer), file);
        if (ret > 0) {
            len = ret;
        }
        printf("private key len read len: %d\n", len);
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
    printf("Private len=%d\n", key_len);
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
    printf("%s", data);
    return edge_sdk::kOk;
}

#define USER_APP_NAME "live"
#define USER_APP_ID "140488"
#define USER_APP_KEY "864ed43d1156a000e89c244017fd25c"
#define USER_APP_LICENSE "GkIzGaYS0SwcQSeP9YqBTuuqGlBqgA1nsUZrW23rZ4bfN1/Y1T9g+9CdjkrsjUXkwnHcS3LhdjJx4NL8b2zeM5mihuIhGM4iSesztClRSaNdPpTXiwlosApf8Zzrvtv8hvU+SHgB52DqZxMIbK8QRjYIw2YTiNl0ruxIC9I0XmB0HmhCXR+8VdlVMPpCTaLuur322GklctKURzBNenEDPiwzNCwUHq4f/nBGTbZDCaPaME4xFEBcpI1YT3Q8Y+ZXMIDhN+ZFNZqcxBTvQkRrxJT1lUi2y9kzU20T/jMyXgUMTa7M3KsRxXt1lJSU6vzbvmfSu0FofxeZUzvSlT9maQ=="
#define USER_DEVELOPER_ACCOUNT "1399028738@qq.com"

static edge_sdk::ErrorCode InitOptions(edge_sdk::Options& option) {
    option.product_name = "Edge-1.0";
    option.vendor_name = "Vendor";
    option.serial_number = "SN0000100010101";
    option.firmware_version = {0, 1, 0, 0};

    edge_sdk::AppInfo app_info;
    app_info.app_name =
        std::string((char*)USER_APP_NAME, strlen(USER_APP_NAME));
    app_info.app_id = std::string((char*)USER_APP_ID, strlen(USER_APP_ID));
    app_info.app_key = std::string((char*)USER_APP_KEY, strlen(USER_APP_KEY));
    app_info.app_license =
        std::string((char*)USER_APP_LICENSE, strlen(USER_APP_LICENSE));

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