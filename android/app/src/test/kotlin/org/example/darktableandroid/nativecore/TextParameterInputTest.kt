/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.nativecore

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TextParameterInputTest {
    private val control = ModuleTextParameter("text", "Watermark text", "", 10)

    @Test fun utf8PayloadLimitCountsMultibyteCharactersAndEmoji() {
        assertTrue(control.accepts(""))
        assertTrue(control.accepts("相片📷")) // 3 + 3 + 4 bytes, exactly the native limit.
        assertFalse(control.accepts("相片📷!"))
        assertTrue(control.accepts("0123456789"))
        assertFalse(control.accepts("01234567890"))
    }

    @Test fun embeddedNulAndMalformedSurrogatesNeverReachNativeFixedBuffers() {
        assertFalse(control.accepts("a\u0000b"))
        assertFalse(control.accepts("\uD800"))
        assertFalse(control.accepts("\uDC00"))
        assertFalse(control.accepts("\uD800a"))
        assertTrue(control.accepts("\uD83D\uDCF7"))
    }

    @Test fun templateFileNamesUseTheNativeResourceBasenameRule() {
        val template = control.copy(name = "filename", maxBytes = 64)
        assertTrue(template.accepts("darktable.svg"))
        listOf("", ".", "..", "/tmp/template.svg", "folder/template.svg", "folder\\template.svg").forEach {
            assertFalse(template.accepts(it))
        }
    }

    @Test fun textMetadataEnablesTextOnlyModulesAndRejectsMalformedLimitsOrDefaults() {
        val module = parseModules("""[{
            "operation":"watermark", "name":"watermark", "enabled":true,
            "textParameters":[
              {"name":"text","label":"Watermark text","value":"Current","maxBytes":10,"default":"Original"},
              {"name":"font","value":"Sans","maxBytes":4,"default":"Too long"},
              {"name":"bad","value":"","maxBytes":4.5}
            ]
        }]""").single()
        assertTrue(module.hasEditableControls)
        assertEquals(2, module.textParameters.size)
        assertEquals("Original", module.textParameters[0].defaultValue)
        assertNull(module.textParameters[1].defaultValue)
        assertEquals(emptyList<ModuleTextParameter>(), parseModules("""[
          {"operation":"old","name":"older runtime","enabled":true}
        ]""").single().textParameters)
    }
}
