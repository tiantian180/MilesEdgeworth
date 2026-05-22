#include "SecretStore.h"

#ifdef MILES_HAS_MAC_SECRET_STORE
#include "MacSecretStore.h"
#endif

std::unique_ptr<SecretStore> SecretStore::create()
{
#ifdef MILES_HAS_MAC_SECRET_STORE
    return std::make_unique<MacSecretStore>();
#else
    return std::make_unique<NullSecretStore>();
#endif
}
