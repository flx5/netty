#include "io_netty_resolver_dns_windows_NativeWrapper.h"

JNIEXPORT jobjectArray JNICALL Java_io_netty_resolver_dns_windows_NativeWrapper_resolvers(JNIEnv* env, jclass clazz) {
    jclass dnsResolverClass = FindClass(env, "io/netty/resolver/dns/windows/DnsResolver");
    jobjectArray array = NewObjectArray(env, 1, dnsResolverClass, NULL);
    if (array == NULL) {
        goto error;
    }

    // TODO Free resources if necessary
    return array;
error:
    // TODO Free resources if necessary
    return NULL;
  }