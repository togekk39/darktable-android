/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid.nativecore

import org.json.JSONArray
import org.json.JSONObject

data class ParameterChoice(val value: Double, val label: String)
data class ModuleParameter(
    val name: String,
    val label: String,
    val type: String,
    val value: Double,
    val minimum: Double?,
    val maximum: Double?,
    val choices: List<ParameterChoice>,
)
data class EditorModule(
    val operation: String,
    val name: String,
    val enabled: Boolean,
    val canDisable: Boolean,
    val parameters: List<ModuleParameter>,
    val curves: List<ModuleCurve> = emptyList(),
    val crop: ModuleCrop? = null,
    val rgbCurveOptions: RgbCurveOptions? = null,
    val whiteBalance: WhiteBalance? = null,
)
data class ModuleCrop(val bounds: CropRect, val minSize: Float, val freehand: Boolean)
data class ModuleCurve(
    val channel: Int,
    val label: String,
    val editable: Boolean,
    val interpolation: Int,
    val maxNodes: Int,
    val minSpacing: Float,
    val nodes: List<CurvePoint>,
    val samples: List<CurvePoint>,
)
data class ExportFormat(
    val id: String,
    val name: String,
    val extension: String,
    val mimeType: String,
    val bitDepth: Int?,
    val options: ExportOptions? = null,
)

internal fun parseModules(json: String): List<EditorModule> = JSONArray(json).let { modules ->
    List(modules.length()) { index ->
        val module = modules.getJSONObject(index)
        val parameters = module.optJSONArray("parameters") ?: JSONArray()
        val curves = module.optJSONArray("curves") ?: JSONArray()
        EditorModule(module.getString("operation"), module.getString("name"), module.getBoolean("enabled"), module.optBoolean("canDisable", true),
            List(parameters.length()) { parameterIndex ->
                val parameter = parameters.getJSONObject(parameterIndex)
                val choices = parameter.optJSONArray("choices") ?: JSONArray()
                ModuleParameter(
                    parameter.getString("name"), parameter.optString("label", parameter.getString("name")),
                    parameter.getString("type"), parameter.getDouble("value"),
                    parameter.finiteNumber("min"), parameter.finiteNumber("max"),
                    List(choices.length()) { choiceIndex ->
                        val choice = choices.getJSONObject(choiceIndex)
                        ParameterChoice(choice.getDouble("value"), choice.getString("label"))
                    },
                )
            },
            // An unavailable or older curve schema must not hide the image's
            // remaining modules and supported controls.
            (0 until curves.length()).mapNotNull { curveIndex ->
                curves.optJSONObject(curveIndex)?.readCurve()
            }.distinctBy { it.channel },
            module.optJSONObject("crop")?.readCrop(),
            module.optJSONObject("rgbCurveOptions")?.readRgbCurveOptions(),
            module.optJSONObject("whiteBalance")?.readWhiteBalance(),
        )
    }
}

internal fun parseExportFormats(json: String): List<ExportFormat> = JSONArray(json).let { formats ->
    List(formats.length()) { index ->
        val format = formats.getJSONObject(index)
        ExportFormat(
            format.optString("id", format.getString("name")),
            format.optString("label", format.getString("name")),
            format.getString("extension"),
            format.optString("mimeType", "application/octet-stream").takeIf { '/' in it } ?: "application/octet-stream",
            format.optInt("bitDepth", 0).takeIf { it > 0 },
            format.optJSONObject("options")?.let(::parseExportOptions),
        )
    }
}

private fun JSONObject.finiteNumber(name: String): Double? =
    if(has(name) && !isNull(name)) optDouble(name).takeIf { it.isFinite() } else null

private fun JSONObject.readWhiteBalance(): WhiteBalance? = try {
    val channels = getJSONArray("channels")
    val settings = getJSONArray("settings")
    require(channels.length() in 3..4 && settings.length() in 0..3)
    checkedWhiteBalance(List(channels.length()) { index ->
        val channel = channels.getJSONObject(index)
        WhiteBalanceChannel(channel.curveOptionInt("index"), channel.getString("label"), channel.getDouble("value").toFloat())
    }, getDouble("manualMin").toFloat(), getDouble("manualMax").toFloat(), curveOptionInt("setting"),
        getString("settingLabel"), List(settings.length()) { index ->
            val choice = settings.getJSONObject(index)
            WhiteBalanceSetting(choice.curveOptionInt("id"), choice.getString("label"))
        }, optJSONObject("temperature")?.readWhiteBalanceTemperature())
} catch(_: org.json.JSONException) {
    null
} catch(_: IllegalArgumentException) {
    null
}

private fun JSONObject.readWhiteBalanceTemperature(): WhiteBalanceTemperature? = try {
    checkedWhiteBalanceTemperature(getDouble("kelvin"), getDouble("tint"), getDouble("minKelvin"),
        getDouble("maxKelvin"), getDouble("minTint"), getDouble("maxTint"))
} catch(_: org.json.JSONException) {
    null
}

private fun JSONObject.readRgbCurveOptions(): RgbCurveOptions? = try {
    checkedRgbCurveOptions(curveOptionInt("mode"), curveOptionInt("preserveColors"),
        getJSONArray("modes").curveOptionChoices(), getJSONArray("preserveColorChoices").curveOptionChoices())
} catch(_: org.json.JSONException) {
    null
} catch(_: IllegalArgumentException) {
    null
}

private fun JSONObject.curveOptionInt(name: String): Int {
    val value = (get(name) as? Number)?.toDouble() ?: throw IllegalArgumentException("Expected an integer")
    require(value.isFinite() && value == value.toInt().toDouble())
    return value.toInt()
}

private fun JSONArray.curveOptionChoices(): List<CurveOptionChoice> {
    require(length() in 1..64)
    return List(length()) { index ->
        val choice = getJSONObject(index)
        CurveOptionChoice(choice.curveOptionInt("value"), choice.getString("label"))
    }
}

private fun JSONObject.readCrop(): ModuleCrop? = try {
    val bounds = CropRect(getDouble("left").toFloat(), getDouble("top").toFloat(),
        getDouble("right").toFloat(), getDouble("bottom").toFloat())
    val minSize = getDouble("minSize").toFloat()
    require(validCropRect(bounds, minSize))
    ModuleCrop(bounds, minSize, getBoolean("freehand"))
} catch(_: org.json.JSONException) {
    null
} catch(_: IllegalArgumentException) {
    null
}

private fun JSONObject.readCurve(): ModuleCurve? = try {
    val nodes = getJSONArray("nodes").curvePoints()
    val maxNodes = getInt("maxNodes")
    val minSpacing = optDouble("minSpacing", 0.000001).toFloat()
    require(validCurveNodes(nodes, maxNodes, minSpacing))
    val interpolation = getInt("interpolation")
    val channel = getInt("channel")
    require(interpolation in 0..2 && channel in 0..2)
    val samples = getJSONArray("samples").curvePoints()
    require(samples.size in 2..4096 && samples.all { it.x in 0f..1f } &&
        samples.zipWithNext().all { (left, right) -> right.x > left.x })
    ModuleCurve(channel, getString("label"), getBoolean("editable"), interpolation,
        maxNodes, minSpacing, nodes, samples)
} catch(_: org.json.JSONException) {
    null
} catch(_: IllegalArgumentException) {
    null
}

private fun JSONArray.curvePoints(): List<CurvePoint> = List(length()) { index ->
    val point = getJSONObject(index)
    val x = point.getDouble("x").toFloat()
    val y = point.getDouble("y").toFloat()
    require(x.isFinite() && y.isFinite()) { "Invalid native curve coordinates" }
    CurvePoint(x, y)
}
