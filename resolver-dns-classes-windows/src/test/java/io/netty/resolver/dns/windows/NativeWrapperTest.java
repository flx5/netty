package io.netty.resolver.dns.windows;


import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.condition.EnabledOnOs;
import org.junit.jupiter.api.condition.OS;

import static org.assertj.core.api.Assertions.assertThat;

@EnabledOnOs(OS.WINDOWS)
public class NativeWrapperTest {
    @Test
    void testNativeCall() {
        DnsResolver[] resolvers = NativeWrapper.resolvers();
        assertThat(resolvers).hasSize(1);
    }
}
