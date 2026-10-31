/* SPDX-License-Identifier: GPL-3.0-or-later */
package org.example.darktableandroid

import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.test.assertIsDisplayed
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.compose.ui.test.onNodeWithContentDescription
import androidx.compose.ui.test.onNodeWithText
import androidx.compose.ui.test.performClick
import androidx.test.platform.app.InstrumentationRegistry
import org.example.darktableandroid.nativecore.ModuleParameter
import org.junit.Rule
import org.junit.Test

class MaskDiagramTest {
    @get:Rule val compose = createComposeRule()
    private val context get() = InstrumentationRegistry.getInstrumentation().targetContext

    private fun field(id: Int, name: String, value: Double, shape: String) =
        ModuleParameter("mask.$id.$name", "$shape $id · $name", "float", value, null, null, emptyList())

    @Test fun nativeShapeSelectionKeepsDiagramAndAccessibleCoordinatesTogether() {
        val parameters = listOf(
            field(15, "x", 0.25, "circle"), field(15, "y", 0.5, "circle"), field(15, "radius", 0.1, "circle"),
            field(15, "source_x", 0.75, "circle"), field(15, "source_y", 0.5, "circle"),
            field(27, "x", 0.7, "ellipse"), field(27, "y", 0.4, "ellipse"),
            field(27, "radius_x", 0.2, "ellipse"), field(27, "radius_y", 0.1, "ellipse"),
        )
        compose.setContent { MaterialTheme { MaskDiagram(parameters) } }
        val circleDescription = context.getString(R.string.mask_diagram_description, "circle 15", "0.25", "0.5") + " " +
            context.getString(R.string.mask_diagram_source, "0.75", "0.5")
        compose.onNodeWithContentDescription(circleDescription).assertIsDisplayed()
        compose.onNodeWithText("circle 15").performClick()
        compose.onNodeWithText("ellipse 27").performClick()
        compose.onNodeWithContentDescription(context.getString(R.string.mask_diagram_description, "ellipse 27", "0.7", "0.4"))
            .assertIsDisplayed()
    }
}
