/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import org.example.darktableandroid.nativecore.ModuleParameter
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class MaskDiagramGeometryTest {
    private fun field(id: Int, name: String, value: Double, label: String = "circle $id · $name") =
        ModuleParameter("mask.$id.$name", label, "float", value, null, null, emptyList())

    @Test fun nativeStableIdsKeepGeometryAndCloneSourcesWithTheCorrectShape() {
        val parameters = listOf(
            field(15, "radius", 0.1), field(15, "x", 0.25), field(15, "y", 0.5),
            field(15, "source_x", 0.75), field(15, "source_y", 0.5),
            field(27, "radius_x", 0.2), field(27, "radius_y", 0.1),
            field(27, "x", 0.7, "ellipse 27 · center x"), field(27, "y", 0.4), field(27, "rotation", 45.0),
        )
        val shapes = maskDiagramShapes(parameters)
        assertEquals(listOf(15, 27), shapes.map { it.id })
        assertEquals(MaskDiagramKind.CIRCLE, shapes[0].kind)
        assertEquals(0.75f, requireNotNull(shapes[0].sourceX), 0f)
        assertEquals(0.5f, requireNotNull(shapes[0].sourceY), 0f)
        assertEquals("ellipse 27", shapes[1].label)
        assertEquals(MaskDiagramKind.ELLIPSE, shapes[1].kind)
        assertEquals(45f, shapes[1].rotation, 0f)
    }

    @Test fun gradientAnchorIsRecognizedWithoutTreatingActionsOrIncompleteDataAsShapes() {
        val parameters = listOf(
            ModuleParameter("mask.add.circle", "add circle mask", "action", 1.0, 1.0, 1.0, emptyList()),
            field(8, "x", 0.2), field(8, "y", 0.3), field(8, "compression", 0.5), field(8, "rotation", -90.0),
            field(9, "x", 0.2), field(9, "radius", 0.1), // Missing native y.
            field(10, "x", Double.NaN), field(10, "y", 0.5), field(10, "radius", 0.1),
        )
        val shape = maskDiagramShapes(parameters).single()
        assertEquals(8, shape.id)
        assertEquals(MaskDiagramKind.GRADIENT, shape.kind)
        assertEquals(-90f, shape.rotation, 0f)
        assertTrue(maskDiagramShapes(emptyList()).isEmpty())
    }
}
