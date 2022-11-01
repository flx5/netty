/*
 * Copyright 2022 The Netty Project
 *
 * The Netty Project licenses this file to you under the Apache License,
 * version 2.0 (the "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at:
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
package io.netty.resolver.dns.windows;

import io.netty.util.internal.ClassInitializerUtil;
import io.netty.util.internal.NativeLibraryLoader;
import io.netty.util.internal.PlatformDependent;
import io.netty.util.internal.ThrowableUtil;
import io.netty.util.internal.logging.InternalLogger;
import io.netty.util.internal.logging.InternalLoggerFactory;

public final class NativeWrapper {
    private static final Throwable UNAVAILABILITY_CAUSE;
    private static final InternalLogger logger =
            InternalLoggerFactory.getInstance(NativeWrapper.class);

    static {
        // Preload all classes that will be used in the OnLoad(...) function of JNI to eliminate the possiblity of a
        // class-loader deadlock. This is a workaround for https://github.com/netty/netty/issues/11209.

        // This needs to match all the classes that are loaded via NETTY_JNI_UTIL_LOAD_CLASS or looked up via
        // NETTY_JNI_UTIL_FIND_CLASS.
        // TODO Figure out which classes are required here
        ClassInitializerUtil.tryLoadClasses(DnsResolver.class, byte[].class, String.class
        );

        Throwable cause = null;
        try {
            loadNativeLibrary();
        } catch (Throwable error) {
            cause = error;
        }
        UNAVAILABILITY_CAUSE = cause;
    }

    private NativeWrapper() { }

    private static void loadNativeLibrary() {
        if (!PlatformDependent.isWindows()) {
            throw new IllegalStateException("Only supported on Windows");
        }

        String sharedLibName = "netty_resolver_dns_native_windows" + '_' + PlatformDependent.normalizedArch();
        ClassLoader cl = PlatformDependent.getClassLoader(NativeWrapper.class);
        NativeLibraryLoader.load(sharedLibName, cl, "windows" + PlatformDependent.bitMode());
    }

    public static boolean isAvailable() {
        return UNAVAILABILITY_CAUSE == null;
    }

    public static void ensureAvailability() {
        if (UNAVAILABILITY_CAUSE != null) {
            throw (Error) new UnsatisfiedLinkError(
                    "failed to load the required native library").initCause(UNAVAILABILITY_CAUSE);
        }
    }

    public static Throwable unavailabilityCause() {
        return UNAVAILABILITY_CAUSE;
    }
    public static native DnsResolver[] resolvers();
}
