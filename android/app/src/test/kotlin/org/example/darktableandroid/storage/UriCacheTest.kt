/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.storage

import java.nio.file.Files
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class UriCacheTest {
    @Test
    fun pruneAfterFinalizationRetainsNewSourceWithinCacheLimit() {
        val directory = Files.createTempDirectory("uri-cache-test").toFile()
        try {
            val oldSource = directory.resolve("old.raw").apply {
                writeBytes(ByteArray(6))
                setLastModified(1L)
            }
            val newSource = directory.resolve("new.raw").apply {
                writeBytes(ByteArray(6))
                setLastModified(2L)
            }

            pruneCache(directory, maxBytes = 10L, retained = newSource)

            assertFalse(oldSource.exists())
            assertTrue(newSource.exists())
            assertTrue(directory.listFiles().orEmpty().sumOf { it.length() } <= 10L)
        } finally {
            directory.deleteRecursively()
        }
    }
}
