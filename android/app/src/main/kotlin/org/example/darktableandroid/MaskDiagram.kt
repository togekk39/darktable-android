/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import android.graphics.Paint
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import org.example.darktableandroid.nativecore.ModuleParameter
import kotlin.math.cos
import kotlin.math.sin

internal enum class MaskDiagramKind { CIRCLE, ELLIPSE, GRADIENT }

internal data class MaskDiagramShape(
    val id: Int, val label: String, val kind: MaskDiagramKind,
    val x: Float, val y: Float, val radiusX: Float, val radiusY: Float,
    val rotation: Float, val sourceX: Float?, val sourceY: Float?, val enabled: Boolean,
)

private val maskShapePath = Regex("^mask\\.([1-9][0-9]*)\\.(.+)$")

/** Read only named native geometry descriptors; action paths and unrelated fields are ignored. */
internal fun maskDiagramShapes(parameters: List<ModuleParameter>): List<MaskDiagramShape> {
    val groups = linkedMapOf<Int, MutableMap<String, ModuleParameter>>()
    parameters.forEach { parameter ->
        val match = maskShapePath.matchEntire(parameter.name) ?: return@forEach
        val id = match.groupValues[1].toIntOrNull() ?: return@forEach
        groups.getOrPut(id) { linkedMapOf() }[match.groupValues[2]] = parameter
    }
    return groups.mapNotNull { (id, fields) ->
        fun number(name: String): Float? = fields[name]?.value?.toFloat()?.takeIf { it.isFinite() && it in -100f..100f }
        val x = number("x") ?: return@mapNotNull null
        val y = number("y") ?: return@mapNotNull null
        val radius = number("radius")
        val radiusX = number("radius_x")
        val radiusY = number("radius_y")
        val kind = when {
            radius != null && radius > 0f -> MaskDiagramKind.CIRCLE
            radiusX != null && radiusX > 0f && radiusY != null && radiusY > 0f -> MaskDiagramKind.ELLIPSE
            number("compression") != null -> MaskDiagramKind.GRADIENT
            else -> return@mapNotNull null
        }
        val label = fields.getValue("x").label.substringBefore('·').trim()
        MaskDiagramShape(id, label, kind, x, y, radius ?: radiusX ?: 0f, radius ?: radiusY ?: 0f,
            number("rotation") ?: 0f, number("source_x"), number("source_y"), fields["enabled"]?.value != 0.0)
    }
}

/** A square coordinate schematic, separate from the processed photo and its distortions. */
@Composable
internal fun MaskDiagram(parameters: List<ModuleParameter>) {
    val shapes = maskDiagramShapes(parameters)
    if(shapes.isEmpty()) return
    var selectedId by rememberSaveable { mutableStateOf(shapes.first().id) }
    var expanded by rememberSaveable { mutableStateOf(false) }
    val shape = shapes.firstOrNull { it.id == selectedId } ?: shapes.first()
    val gridColor = MaterialTheme.colorScheme.outlineVariant
    val shapeColor = MaterialTheme.colorScheme.primary.copy(alpha = if(shape.enabled) 1f else 0.4f)
    val sourceColor = MaterialTheme.colorScheme.tertiary
    var description = stringResource(R.string.mask_diagram_description, shape.label, shape.x.toString(), shape.y.toString())
    if(shape.sourceX != null && shape.sourceY != null)
        description += " " + stringResource(R.string.mask_diagram_source, shape.sourceX.toString(), shape.sourceY.toString())
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        if(shapes.size > 1) Box {
            OutlinedButton(onClick = { expanded = true }) { Text(shape.label) }
            DropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                shapes.forEach { item ->
                    DropdownMenuItem(text = { Text(item.label) }, onClick = { selectedId = item.id; expanded = false })
                }
            }
        } else Text(shape.label, style = MaterialTheme.typography.labelLarge)
        Canvas(Modifier.fillMaxWidth().height(200.dp).background(MaterialTheme.colorScheme.surfaceVariant)
            .semantics { contentDescription = description }) {
            val padding = 16.dp.toPx()
            val extent = (minOf(size.width, size.height) - padding * 2).coerceAtLeast(1f)
            val left = (size.width - extent) / 2f
            val top = (size.height - extent) / 2f
            fun position(x: Float, y: Float) = Offset(left + x * extent, top + y * extent)
            for(step in 0..4) {
                val fraction = step / 4f
                drawLine(gridColor, position(fraction, 0f), position(fraction, 1f))
                drawLine(gridColor, position(0f, fraction), position(1f, fraction))
            }
            val center = position(shape.x, shape.y)
            val line = 2.dp.toPx()
            val marker = 5.dp.toPx()
            val dashed = PathEffect.dashPathEffect(floatArrayOf(5.dp.toPx(), 4.dp.toPx()))
            clipRect(left, top, left + extent, top + extent) {
                when(shape.kind) {
                    MaskDiagramKind.CIRCLE -> drawCircle(shapeColor, shape.radiusX * extent, center, style = Stroke(line))
                    MaskDiagramKind.ELLIPSE -> rotate(shape.rotation, center) {
                        drawOval(shapeColor, Offset(center.x - shape.radiusX * extent, center.y - shape.radiusY * extent),
                            Size(shape.radiusX * extent * 2f, shape.radiusY * extent * 2f), style = Stroke(line))
                    }
                    MaskDiagramKind.GRADIENT -> {
                        // This marks the native anchor and tangent direction; it does not
                        // attempt to reproduce the processed curved-gradient mask.
                        val angle = Math.toRadians(-shape.rotation.toDouble())
                        val direction = Offset(cos(angle).toFloat(), sin(angle).toFloat()) * (extent * 0.4f)
                        drawLine(shapeColor, center - direction, center + direction, line)
                    }
                }
                if(shape.sourceX != null && shape.sourceY != null) {
                    val source = position(shape.sourceX, shape.sourceY)
                    drawLine(sourceColor, source, center, 1.dp.toPx(), pathEffect = dashed)
                    drawCircle(sourceColor, marker, source, style = Stroke(line))
                    drawLine(sourceColor, source - Offset(marker, 0f), source + Offset(marker, 0f), line)
                    drawLine(sourceColor, source - Offset(0f, marker), source + Offset(0f, marker), line)
                }
                drawCircle(shapeColor, 2.5.dp.toPx(), center)
                val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = shapeColor.toArgb(); textSize = 11.sp.toPx() }
                drawContext.canvas.nativeCanvas.drawText(shape.id.toString(), center.x + marker, center.y - marker, paint)
            }
        }
        Text(stringResource(R.string.mask_diagram_hint), style = MaterialTheme.typography.bodySmall)
    }
}
