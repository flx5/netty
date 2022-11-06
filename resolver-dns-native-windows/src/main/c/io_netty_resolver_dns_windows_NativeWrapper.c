#include "io_netty_resolver_dns_windows_NativeWrapper.h"
#include <winsock2.h>
#include <iphlpapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <Ws2ipdef.h>
#include <ws2tcpip.h>

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")
#pragma comment(lib, "ws2_32.lib")

#define WORKING_BUFFER_SIZE 15000
#define MAX_TRIES 3

#ifndef NETTY_JNI_UTIL_JNI_VERSION
#define NETTY_JNI_UTIL_JNI_VERSION JNI_VERSION_1_6
#endif

jclass dnsResolverClass;
jmethodID dnsResolverCtor;
jclass exceptionClass;

jclass inetSocketAddressClass;
jmethodID inetSocketAddressCtor;


IP_ADAPTER_ADDRESSES *read_adapter_addresses(JNIEnv *env) {
    unsigned long outBufLen = WORKING_BUFFER_SIZE;
    IP_ADAPTER_ADDRESSES *pAddresses;
    unsigned long dwRetVal;

    for (int tries = 0; tries < MAX_TRIES; ++tries) {
        pAddresses = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);

        if (pAddresses == NULL) {
            // This probably won't work either...
            fprintf(stderr, "Could not allocate address buffer");
            (*env)->ThrowNew(env, exceptionClass, "Could not allocate address buffer");
            return NULL;
        }

        ULONG flags = GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST
                      | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME;

        // This call can update the outBufLen variable with the actual (required) size.
        dwRetVal = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAddresses, &outBufLen);

        if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
            free(pAddresses);
            pAddresses = NULL;
        } else {
            break;
        }
    }

    if (dwRetVal != NO_ERROR) {
        free(pAddresses);
        return NULL;
    }

    return pAddresses;
}

jobject socket_to_java(JNIEnv *env, LPSOCKADDR sock_address) {
    switch (sock_address->sa_family) {
        case AF_INET: {
            char ip[INET_ADDRSTRLEN];

            struct sockaddr_in *addr_in = (struct sockaddr_in *) sock_address;

            inet_ntop(AF_INET, &addr_in->sin_addr, ip, sizeof(ip));
            jint port = ntohs(addr_in->sin_port);

            jstring java_ip = (*env)->NewStringUTF(env, ip);

            return (*env)->NewObject(env, inetSocketAddressClass, inetSocketAddressCtor, java_ip, port);
        }
        case AF_INET6: {
            char ip[INET6_ADDRSTRLEN];
            struct sockaddr_in6 *addr_in = (struct sockaddr_in6 *) sock_address;
            inet_ntop(AF_INET6, &addr_in->sin6_addr, ip, sizeof(ip));
            jint port = ntohs(addr_in->sin6_port);

            jstring java_ip = (*env)->NewStringUTF(env, ip);
            return (*env)->NewObject(env, inetSocketAddressClass, inetSocketAddressCtor, java_ip, port);
        }
        default: {
            fprintf(stderr, "Unknown address family: %d", sock_address->sa_family);
            (*env)->ThrowNew(env, exceptionClass, "Unknown address family");
            return NULL;
        }
    }
}

jobjectArray get_dns_servers(JNIEnv *env, PIP_ADAPTER_ADDRESSES currentAddresses) {
    jsize dns_server_count = 0;

    for (IP_ADAPTER_DNS_SERVER_ADDRESS *dnsServer = currentAddresses->FirstDnsServerAddress;
         dnsServer != NULL; dnsServer = dnsServer->Next) {
        ++dns_server_count;
    }

    jobjectArray array = (*env)->NewObjectArray(env, dns_server_count, inetSocketAddressClass, NULL);
    if (array == NULL) {
        fprintf(stderr, "Could not allocate dns server array with size %d", dns_server_count);
        (*env)->ThrowNew(env, exceptionClass, "Could not allocate dns server array.");
        return NULL;
    }

    int array_index = 0;

    for (IP_ADAPTER_DNS_SERVER_ADDRESS *dnsServer = currentAddresses->FirstDnsServerAddress;
     dnsServer != NULL; dnsServer = dnsServer->Next) {
        LPSOCKADDR sock_address = dnsServer->Address.lpSockaddr;

        (*env)->SetObjectArrayElement(env, array, array_index++, socket_to_java(env, sock_address));
    }

    return array;
}

jobjectArray get_adapter_information(JNIEnv *env) {
    IP_ADAPTER_ADDRESSES *pAddresses = read_adapter_addresses(env);

    if (pAddresses == NULL) {
        return NULL;
    }

    int interface_counter = 0;
    for (PIP_ADAPTER_ADDRESSES currentAddresses = pAddresses; currentAddresses != NULL;
         currentAddresses = currentAddresses->Next) {
        ++interface_counter;
    }

    jobjectArray array = (*env)->NewObjectArray(env, interface_counter, dnsResolverClass, NULL);
    if (array == NULL) {
        free(pAddresses);
        return NULL;
    }

    int interface_index = 0;
    for (PIP_ADAPTER_ADDRESSES currentAddresses = pAddresses; currentAddresses != NULL;
         currentAddresses = currentAddresses->Next) {

        jobjectArray dns_servers = get_dns_servers(env, currentAddresses);

        jobject java_resolver = (*env)->NewObject(env, dnsResolverClass, dnsResolverCtor, dns_servers);
        (*env)->SetObjectArrayElement(env, array, interface_index++, java_resolver);
    }

    free(pAddresses);

    return array;
}

JNIEXPORT jobjectArray JNICALL Java_io_netty_resolver_dns_windows_NativeWrapper_resolvers(JNIEnv *env, jclass clazz) {
    return get_adapter_information(env);
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void *reserved) {
    JNIEnv *env = NULL;
    if ((*vm)->GetEnv(vm, (void**) &env, NETTY_JNI_UTIL_JNI_VERSION) != JNI_OK) {
        fprintf(stderr,
        "FATAL: JNI version mismatch");
        fflush(stderr);
        return JNI_ERR;
    }

    dnsResolverClass = (*env)->FindClass(env, "io/netty/resolver/dns/windows/DnsResolver");
    dnsResolverCtor = (*env)->GetMethodID(env, dnsResolverClass, "<init>", "([Ljava/net/InetSocketAddress;)V");

    inetSocketAddressClass = (*env)->FindClass(env, "java/net/InetSocketAddress");
    inetSocketAddressCtor = (*env)->GetMethodID(env, inetSocketAddressClass, "<init>", "(Ljava/lang/String;I)V");

    exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");

    return NETTY_JNI_UTIL_JNI_VERSION;
}