#ifndef TINY_CRYPT_H
#define TINY_CRYPT_H

#include <string>

namespace TinyCrypt
{

class CryptObject {
public:
    CryptObject(const unsigned char* key);
    ~CryptObject();
    std::string encrypt(const std::string& text);
    std::string decrypt(const std::string& text);

private:
    void* m_ctx;
};

} // namespace TinyCrypt

#endif // TINY_CRYPT_H