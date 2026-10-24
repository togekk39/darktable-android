/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import org.example.darktableandroid.nativecore.*

@Composable
internal fun CropEditor(crop: ModuleCrop, editable: Boolean, onApply: (CropRect) -> Unit) {
    var left by rememberSaveable(crop) { mutableStateOf(crop.bounds.left.toString()) }
    var top by rememberSaveable(crop) { mutableStateOf(crop.bounds.top.toString()) }
    var right by rememberSaveable(crop) { mutableStateOf(crop.bounds.right.toString()) }
    var bottom by rememberSaveable(crop) { mutableStateOf(crop.bounds.bottom.toString()) }
    val draft = parseCropInput(left, top, right, bottom, crop.minSize)
    val changed = draft != crop.bounds || !crop.freehand

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Text(stringResource(R.string.crop_freehand), style = MaterialTheme.typography.titleSmall)
        CropDiagram(crop.bounds, draft)
        Text(stringResource(R.string.crop_diagram_hint), style = MaterialTheme.typography.bodySmall)
        if(!crop.freehand) Text(stringResource(R.string.crop_freehand_hint), style = MaterialTheme.typography.bodySmall)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            CropCoordinate(left, { left = it }, stringResource(R.string.crop_left), editable, draft != null, Modifier.weight(1f))
            CropCoordinate(top, { top = it }, stringResource(R.string.crop_top), editable, draft != null, Modifier.weight(1f))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            CropCoordinate(right, { right = it }, stringResource(R.string.crop_right), editable, draft != null, Modifier.weight(1f))
            CropCoordinate(bottom, { bottom = it }, stringResource(R.string.crop_bottom), editable, draft != null, Modifier.weight(1f))
        }
        Text(stringResource(R.string.crop_coordinate_range, crop.minSize.toString()),
            style = MaterialTheme.typography.bodySmall)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TextButton(onClick = { left = "0.0"; top = "0.0"; right = "1.0"; bottom = "1.0" }, enabled = editable) {
                Text(stringResource(R.string.crop_reset))
            }
            Button(onClick = { draft?.let(onApply) }, enabled = editable && draft != null && changed) {
                Text(stringResource(R.string.crop_apply))
            }
        }
    }
}

@Composable
private fun CropCoordinate(value: String, onValue: (String) -> Unit, label: String,
                           editable: Boolean, valid: Boolean, modifier: Modifier) {
    OutlinedTextField(value = value, onValueChange = onValue, label = { Text(label) }, modifier = modifier,
        enabled = editable, singleLine = true, isError = !valid,
        keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Ascii))
}

@Composable
private fun CropDiagram(applied: CropRect, draft: CropRect?) {
    val gridColor = MaterialTheme.colorScheme.outlineVariant
    val appliedColor = MaterialTheme.colorScheme.primary
    val draftColor = MaterialTheme.colorScheme.tertiary
    val description = stringResource(R.string.crop_diagram_description)
    Canvas(Modifier.fillMaxWidth().height(200.dp).background(MaterialTheme.colorScheme.surfaceVariant)
        .semantics { contentDescription = description }) {
        val padding = 12.dp.toPx()
        val width = (size.width - padding * 2).coerceAtLeast(1f)
        val height = (size.height - padding * 2).coerceAtLeast(1f)
        for(step in 0..4) {
            val fraction = step / 4f
            drawLine(gridColor, Offset(padding + fraction * width, padding), Offset(padding + fraction * width, padding + height))
            drawLine(gridColor, Offset(padding, padding + fraction * height), Offset(padding + width, padding + fraction * height))
        }
        fun origin(rect: CropRect) = Offset(padding + rect.left * width, padding + rect.top * height)
        fun extent(rect: CropRect) = Size((rect.right - rect.left) * width, (rect.bottom - rect.top) * height)
        // This is a normalized coordinate diagram. The already-cropped photo is
        // never drawn as though it were the uncropped crop-module input.
        drawRect(appliedColor, origin(applied), extent(applied), style = Stroke(2.dp.toPx()))
        if(draft != null && draft != applied) {
            drawRect(draftColor, origin(draft), extent(draft), style = Stroke(2.dp.toPx(),
                pathEffect = PathEffect.dashPathEffect(floatArrayOf(6.dp.toPx(), 4.dp.toPx()))))
        }
    }
}
