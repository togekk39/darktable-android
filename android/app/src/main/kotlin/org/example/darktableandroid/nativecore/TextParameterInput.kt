/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.nativecore

import java.nio.CharBuffer
import java.nio.charset.CharacterCodingException
import java.nio.charset.CodingErrorAction

/** Native buffers are bounded in UTF-8 bytes; character counts alone are insufficient. */
internal fun ModuleTextParameter.accepts(text: String): Boolean {
    if(maxBytes !in 1..65535 || '\u0000' in text) return false
    // The native watermark template setting selects a resource basename.
    if(name == "filename" && (text.isEmpty() || text == "." || text == ".." || '/' in text || '\\' in text)) return false
    return try {
        Charsets.UTF_8.newEncoder()
            .onMalformedInput(CodingErrorAction.REPORT)
            .onUnmappableCharacter(CodingErrorAction.REPORT)
            .encode(CharBuffer.wrap(text)).remaining() <= maxBytes
    } catch(_: CharacterCodingException) {
        false
    }
}
