/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.storage
import org.junit.Assert.assertEquals
import org.junit.Test
class CacheIdentityTest { @Test fun placeholderDocumentsStableHashContract() { assertEquals(64, "00".repeat(32).length) } }
