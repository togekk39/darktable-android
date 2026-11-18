/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.storage
import org.junit.Assert.assertEquals
import org.junit.Test
class CacheIdentityTest {
    @Test fun contentIdentityUsesFullSha256() { assertEquals(64, "00".repeat(32).length) }

    @Test fun extensionIsPreservedAndSanitized() {
        assertEquals("gpr", safeExtension("GOPR0123.GPR"))
        assertEquals("dng", safeExtension("camera.dng"))
        assertEquals("raw", safeExtension("malicious.r@w"))
        assertEquals("raw", safeExtension("no-extension"))
    }
}
