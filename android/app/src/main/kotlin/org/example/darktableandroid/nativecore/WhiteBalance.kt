/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.nativecore

data class WhiteBalanceChannel(val index: Int, val label: String, val value: Float)
data class WhiteBalanceSetting(val id: Int, val label: String)
data class WhiteBalance(
    val channels: List<WhiteBalanceChannel>,
    val manualMin: Float,
    val manualMax: Float,
    val setting: Int,
    val settingLabel: String,
    val settings: List<WhiteBalanceSetting>,
) {
    fun acceptsGains(gains: FloatArray): Boolean = gains.size == channels.size &&
        gains.all { it.isFinite() && it in manualMin..manualMax }
    fun acceptsSetting(id: Int): Boolean = id in listOf(0, 3, 4) && settings.any { it.id == id }
}

internal fun checkedWhiteBalance(channels: List<WhiteBalanceChannel>, minimum: Float, maximum: Float,
    setting: Int, label: String, settings: List<WhiteBalanceSetting>): WhiteBalance? {
    if(channels.size !in 3..4 || channels.map { it.index }.toSet() != channels.indices.toSet() ||
        channels.any { it.label.isBlank() || !it.value.isFinite() || it.value <= 0f } ||
        !minimum.isFinite() || !maximum.isFinite() || minimum <= 0f || minimum > maximum ||
        label.isBlank() || settings.map { it.id }.distinct().size != settings.size ||
        settings.any { it.id !in listOf(0, 3, 4) || it.label.isBlank() }) return null
    // An original camera setting can exceed the manual slider range. Keep it
    // visible and unchanged; validate that range only when applying manual gains.
    return WhiteBalance(channels.toList(), minimum, maximum, setting, label, settings.toList())
}

/** Display order may differ from the original coefficient order (e.g. CYGM). */
internal fun whiteBalanceGains(balance: WhiteBalance, input: List<String>): FloatArray? {
    if(input.size != balance.channels.size) return null
    val gains = FloatArray(input.size)
    for((position, channel) in balance.channels.withIndex()) {
        val value = input[position].toFloatOrNull() ?: return null
        if(channel.index !in gains.indices) return null
        gains[channel.index] = value
    }
    return gains.takeIf(balance::acceptsGains)
}
