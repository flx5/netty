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

int get_dns_servers(JNIEnv* env) {
  unsigned long outBufLen = WORKING_BUFFER_SIZE;
  IP_ADAPTER_ADDRESSES * pAddresses;
  unsigned long dwRetVal;


  for (int tries = 0; tries < MAX_TRIES; ++tries) {
    pAddresses = (IP_ADAPTER_ADDRESSES *) malloc(outBufLen);

    if (pAddresses == NULL) {
      // This won't work probably either...
      // TODO Log to stderr also
      (*env)->ThrowNew(env, exceptionClass, "Could not allocate address buffer");
      return 1;
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
    if (dwRetVal == NO_ERROR) {
      for (PIP_ADAPTER_ADDRESSES currentAddresses = pAddresses; currentAddresses != NULL;
        currentAddresses = currentAddresses->Next) {

        for(IP_ADAPTER_DNS_SERVER_ADDRESS*  dnsServer = currentAddresses->FirstDnsServerAddress;
           dnsServer != NULL; dnsServer = dnsServer->Next) {
          LPSOCKADDR sock_address = dnsServer->Address.lpSockaddr;
          // TODO Port is always 0
          switch (sock_address->sa_family) {
            case AF_INET:{
              char ip[INET_ADDRSTRLEN];

              struct sockaddr_in *addr_in = (struct sockaddr_in *)sock_address;

              inet_ntop (AF_INET, &addr_in->sin_addr, ip, sizeof (ip));
              unsigned short port = ntohs (addr_in->sin_port);

              // TODO Remove printf
              printf("DNS Server IPv4 %s Port %hu Raw Port %hu\r\n", ip, port, addr_in->sin_port);
              break;
              }
            case AF_INET6: {
              char ip[INET6_ADDRSTRLEN];
              struct sockaddr_in6 *addr_in = (struct sockaddr_in6*)sock_address;
              inet_ntop (AF_INET6, &addr_in->sin6_addr, ip, sizeof (ip));
              unsigned short port = ntohs (addr_in->sin6_port);
                // TODO Remove printf
              printf("DNS Server IPv6 %s Port %hu\r\n", ip, port);
              break;
              }
          }
        }
      }
    } else {
      // TODO Make sure to free the paddresses here?
      // TODO Improve error message
      (*env)->ThrowNew(env, exceptionClass, "Could not read addresses");
      return JNI_ERR;
    }

    free(pAddresses);

    return JNI_OK;

}

JNIEXPORT jobjectArray JNICALL Java_io_netty_resolver_dns_windows_NativeWrapper_resolvers(JNIEnv* env, jclass clazz) {

    if (get_dns_servers(env)) {
      return NULL;
    }

    jobjectArray array = (*env)->NewObjectArray(env, 1, dnsResolverClass, NULL);
    if (array == NULL) {
      goto error;
    }

    jobject java_resolver = (*env)->NewObject(env, dnsResolverClass, dnsResolverCtor, NULL);
    if (java_resolver == NULL) {
      goto error;
    }

    (*env)->SetObjectArrayElement(env, array, 0, java_resolver);

    // TODO Free resources if necessary
    return array;
error:
    // TODO Free resources if necessary

    return NULL;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void* reserved) {
  JNIEnv* env = NULL;
  if ((*vm)->GetEnv(vm, (void**) &env, NETTY_JNI_UTIL_JNI_VERSION) != JNI_OK) {
    fprintf(stderr, "FATAL: JNI version mismatch");
    fflush(stderr);
    return JNI_ERR;
  }

  dnsResolverClass = (*env)->FindClass(env, "io/netty/resolver/dns/windows/DnsResolver");
  dnsResolverCtor = (*env)->GetMethodID(env, dnsResolverClass, "<init>", "([Ljava/net/InetSocketAddress;)V");

  exceptionClass = (*env)->FindClass(env, "java/lang/RuntimeException");

  return NETTY_JNI_UTIL_JNI_VERSION;
}