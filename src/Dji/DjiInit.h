#ifndef __DJI_INIT_H__
#define __DJI_INIT_H__

#include "Edge-SDK/include/error_code.h"
#include "Edge-SDK/include/init.h"

#include <unistd.h>
#include <memory>
#include<string.h>

namespace mediakit {

class KeyStoreDefault : public edge_sdk::KeyStore {
public:
    KeyStoreDefault();

    virtual ~KeyStoreDefault() override {}

    edge_sdk::ErrorCode RSA2048_GetDERPrivateKey(std::string& private_key) const override;

    edge_sdk::ErrorCode RSA2048_GetDERPublicKey(std::string& public_key) const override;

private:
    bool ReadKeys();
    bool GenerateKeys();

private:
    std::shared_ptr<std::string> rsa2048_public_key_;
    std::shared_ptr<std::string> rsa2048_private_key_;

    uint8_t buffer[1500];
};

edge_sdk::ErrorCode ESDKInit();

edge_sdk::ErrorCode ESDKDeInit();

}
#endif 