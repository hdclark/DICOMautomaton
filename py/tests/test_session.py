import os
import subprocess
import tarfile
import tempfile
import unittest
from pathlib import Path

import dicomautomaton as dcma
import numpy as np


def static_state_payload(state):
    return {
        "cumulative_meterset_weight": state.cumulative_meterset_weight,
        "control_point_index": state.control_point_index,
        "gantry_angle": state.gantry_angle,
        "gantry_rotation_direction": state.gantry_rotation_direction,
        "beam_limiting_device_angle": state.beam_limiting_device_angle,
        "beam_limiting_device_rotation_direction": state.beam_limiting_device_rotation_direction,
        "patient_support_angle": state.patient_support_angle,
        "patient_support_rotation_direction": state.patient_support_rotation_direction,
        "table_top_eccentric_angle": state.table_top_eccentric_angle,
        "table_top_eccentric_rotation_direction": state.table_top_eccentric_rotation_direction,
        "table_top_vertical_position": state.table_top_vertical_position,
        "table_top_longitudinal_position": state.table_top_longitudinal_position,
        "table_top_lateral_position": state.table_top_lateral_position,
        "table_top_pitch_angle": state.table_top_pitch_angle,
        "table_top_pitch_rotation_direction": state.table_top_pitch_rotation_direction,
        "table_top_roll_angle": state.table_top_roll_angle,
        "table_top_roll_rotation_direction": state.table_top_roll_rotation_direction,
        "isocentre_position": state.isocentre_position,
        "jaw_positions_x": state.jaw_positions_x,
        "jaw_positions_y": state.jaw_positions_y,
        "mlc_positions_x": state.mlc_positions_x,
        "metadata": dict(state.metadata),
    }


def dynamic_state_payload(state):
    return {
        "beam_number": state.beam_number,
        "final_cumulative_meterset_weight": state.final_cumulative_meterset_weight,
        "static_states": [static_state_payload(item) for item in state.static_states],
        "metadata": dict(state.metadata),
    }


class SessionTests(unittest.TestCase):
    def load_rtplan_fixture(self):
        source_dir = Path(os.environ["DCMA_SOURCE_DIR"])
        archive_path = source_dir / "artifacts/test_files/20200212_Aria_v13.6.5.10_registered_explicit.txz"
        temporary_directory = tempfile.TemporaryDirectory()
        destination = Path(temporary_directory.name) / "fixture.dcm"
        with tarfile.open(archive_path) as archive:
            member = next(item for item in archive.getmembers() if "/RP." in item.name)
            source = archive.extractfile(member)
            self.assertIsNotNone(source)
            destination.write_bytes(source.read())
        session = dcma.Session()
        session.load([str(destination)])
        self.addCleanup(temporary_directory.cleanup)
        return session

    def test_construct_and_enumerate_operations(self):
        session = dcma.Session()
        self.assertEqual(session.data.image_array_count, 0)
        docs = {operation.name: operation for operation in session.operations()}
        self.assertIn("NoOp", docs)
        self.assertIn("False", docs)
        self.assertIn("Throw", docs["False"].aliases)

    def test_run_and_metadata(self):
        session = dcma.Session()
        self.assertEqual(session.lexicon_filename, "")
        session.run("NoOp")
        self.assertNotEqual(session.lexicon_filename, "")
        session.run("CountObjects", Key="images", ImageSelection="all")
        self.assertEqual(session.metadata["images"], "0")

    def test_failure_becomes_exception(self):
        session = dcma.Session()
        with self.assertRaisesRegex(RuntimeError, "False.*false truthiness signal"):
            session.run("False")
        with self.assertRaisesRegex(RuntimeError, "NotAnOperation"):
            session.run("NotAnOperation")

    def test_run_script(self):
        session = dcma.Session()
        session.run_script("CountObjects(Key=script_count, ImageSelection=all);")
        self.assertEqual(session.metadata["script_count"], "0")
        with self.assertRaises(RuntimeError):
            session.run_script("Not valid script syntax")

    def test_loaded_script_runs_before_explicit_operation(self):
        session = dcma.Session()
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "loaded.dscr"
            script.write_text("CountObjects(Key=loaded, ImageSelection=all);", encoding="utf-8")
            session.load([str(script)])
            session.run("NoOp", Probe="$$loaded")
        self.assertEqual(session.metadata["loaded"], "0")

    def test_multiple_loads_preserve_script_order(self):
        session = dcma.Session()
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.dscr"
            second = Path(directory) / "second.dscr"
            first.write_text("CountObjects(Key=first, ImageSelection=all);", encoding="utf-8")
            second.write_text("CountObjects(Key='$$first', ImageSelection=all);", encoding="utf-8")
            session.load([str(first)])
            session.load([str(second)])
            session.run("NoOp")
        self.assertEqual(session.metadata["first"], "0")
        self.assertEqual(session.metadata["0"], "0")

    def test_partial_load_preserves_discovered_script(self):
        session = dcma.Session()
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "loaded.dscr"
            unsupported = Path(directory) / "unsupported.zzz"
            script.write_text("CountObjects(Key=loaded, ImageSelection=all);", encoding="utf-8")
            unsupported.write_text("not a supported data file", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "file loading failed"):
                session.load([str(script), str(unsupported)])
            session.run("NoOp", Probe="$$loaded")
        self.assertEqual(session.metadata["loaded"], "0")

    def test_load_native_image_fixture(self):
        source_dir = Path(os.environ["DCMA_SOURCE_DIR"])
        fixture = source_dir / "artifacts/test_files/2x2x2_random_positive.3ddose"
        session = dcma.Session()
        session.load([str(fixture)])
        self.assertEqual(session.data.image_array_count, 1)
        self.assertEqual(session.data.image_count(0), 2)
        self.assertEqual(session.data.get_image(0, 0).shape, (2, 2, 1))

    def test_run_many_and_scalar_conversion(self):
        session = dcma.Session()
        session.run_many([
            ("NoOp", {"Boolean": True, "Integer": 12, "Float": 1.25}),
            ("CountObjects", {"Key": "count", "ImageSelection": "all"}),
        ])
        self.assertEqual(session.metadata["count"], "0")

    def test_posix_fork_is_rejected_after_python_initialization(self):
        if os.name == "nt":
            self.skipTest("Windows uses thread-emulated Fork")
        session = dcma.Session()
        with self.assertRaisesRegex(RuntimeError, "unavailable after Python"):
            session.run("Fork")

    def test_embedded_operation_in_initialized_extension_process(self):
        has_python_operation = "Python" in {operation.name for operation in dcma.Session().operations()}
        if os.environ.get("DCMA_EXPECT_PYTHON_EMBED") == "1":
            self.assertTrue(has_python_operation, "embedded Python operation was not registered")
        if not has_python_operation:
            self.skipTest("embedded Python support is disabled")
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / "embedded.py"
            script.write_text(
                "def process(session):\n"
                "    session.set_metadata('embedded_in_extension', 'ok')\n",
                encoding="utf-8",
            )
            session = dcma.Session()
            session.run("Python", Filename=str(script))
            self.assertEqual(session.metadata["embedded_in_extension"], "ok")

    def test_data_view_keeps_session_alive_and_calls_repeat(self):
        session = dcma.Session()
        view = session.data
        del session
        self.assertEqual(view.image_array_count, 0)

        session = dcma.Session()
        for _ in range(10):
            session.run("NoOp")

    def test_numpy_image_round_trip_preserves_geometry_and_metadata(self):
        session = dcma.Session()
        session.run(
            "GenerateSyntheticImages",
            NumberOfImages=2,
            NumberOfRows=2,
            NumberOfColumns=3,
            NumberOfChannels=2,
            SliceThickness=4.5,
            SpacingBetweenSlices=6.5,
            VoxelWidth=1.25,
            VoxelHeight=2.5,
            ImageAnchor="10.0, 20.0, 30.0",
            ImagePosition="1.0, 2.0, 3.0",
            ImageOrientationColumn="1.0, 0.0, 0.0",
            ImageOrientationRow="0.0, 1.0, 0.0",
            VoxelValue=7.0,
            StipleValue=-3.0,
            Metadata="PythonRoundTrip@value with spaces",
        )

        self.assertEqual(session.data.image_array_count, 1)
        self.assertEqual(session.data.image_count(0), 2)
        image = session.data.get_image(0, 0)
        pixels = image.to_numpy()
        self.assertEqual(image.shape, (2, 3, 2))
        self.assertEqual(pixels.shape, (2, 3, 2))
        self.assertEqual(pixels.dtype, np.float32)
        self.assertTrue(pixels.flags.c_contiguous)
        self.assertEqual(image.spacing, (1.25, 2.5, 4.5))
        self.assertEqual(image.anchor, (10.0, 20.0, 30.0))
        self.assertEqual(image.offset, (1.0, 2.0, 3.0))
        self.assertEqual(image.row_direction, (0.0, 1.0, 0.0))
        self.assertEqual(image.column_direction, (1.0, 0.0, 0.0))
        self.assertEqual(image.metadata["PythonRoundTrip"], "value with spaces")
        self.assertEqual(pixels[0, 0, 0], -3.0)
        self.assertEqual(pixels[0, 0, 1], 7.0)
        self.assertEqual(pixels[0, 1, 0], 7.0)
        pixels.fill(42.0)
        self.assertEqual(session.data.get_image(0, 0).to_numpy()[0, 0, 0], -3.0)

        second_image = session.data.get_image(0, 1)
        self.assertEqual(second_image.shape, image.shape)
        self.assertEqual(second_image.spacing, image.spacing)
        self.assertEqual(second_image.offset, (1.0, 2.0, 9.5))
        self.assertEqual(second_image.row_direction, image.row_direction)
        self.assertEqual(second_image.column_direction, image.column_direction)
        self.assertEqual(second_image.metadata["PythonRoundTrip"], "value with spaces")
        self.assertEqual(second_image.metadata["InstanceNumber"], "2")

        replacement = np.arange(12, dtype=np.float32).reshape((2, 3, 2))
        original_geometry = (image.spacing, image.anchor, image.offset,
                             image.row_direction, image.column_direction, image.metadata)
        session.data.set_image_pixels(0, 0, replacement)
        replacement.fill(99.0)
        copied_back = session.data.get_image(0, 0)
        np.testing.assert_array_equal(
            copied_back.to_numpy(),
            np.arange(12, dtype=np.float32).reshape((2, 3, 2)),
        )
        self.assertEqual(
            (copied_back.spacing, copied_back.anchor, copied_back.offset,
             copied_back.row_direction, copied_back.column_direction, copied_back.metadata),
            original_geometry,
        )

        session.run("NegatePixels")
        np.testing.assert_array_equal(
            session.data.get_image(0, 0).to_numpy(),
            -np.arange(12, dtype=np.float32).reshape((2, 3, 2)),
        )
        self.assertEqual(image.to_numpy()[0, 0, 0], -3.0)

    def test_numpy_image_copy_rejects_invalid_access_without_mutation(self):
        session = dcma.Session()
        session.run(
            "GenerateSyntheticImages",
            NumberOfImages=1,
            NumberOfRows=2,
            NumberOfColumns=3,
            NumberOfChannels=2,
        )
        original = session.data.get_image(0, 0).to_numpy()

        with self.assertRaises(IndexError):
            session.data.get_image(1, 0)
        with self.assertRaises(IndexError):
            session.data.get_image(0, 1)
        with self.assertRaisesRegex(ValueError, "rows, columns, channels"):
            session.data.set_image_pixels(0, 0, np.zeros((2, 3), dtype=np.float32))
        with self.assertRaisesRegex(ValueError, "expected"):
            session.data.set_image_pixels(0, 0, np.zeros((3, 2, 2), dtype=np.float32))

        np.testing.assert_array_equal(session.data.get_image(0, 0).to_numpy(), original)

    def test_numpy_contour_round_trip_preserves_grouping(self):
        session = dcma.Session()
        session.run(
            "GenerateSyntheticImages",
            NumberOfImages=2,
            NumberOfRows=2,
            NumberOfColumns=3,
            NumberOfChannels=1,
            Metadata="ContourRoundTrip@source",
        )
        session.run("ContourWholeImages", ROILabel="python contour")

        self.assertTrue(session.data.has_contours)
        self.assertEqual(session.data.contour_collection_count, 1)
        self.assertEqual(session.data.contour_count(0), 2)
        contour = session.data.get_contour(0, 0)
        points = contour.to_numpy()
        self.assertEqual(points.ndim, 2)
        self.assertEqual(points.shape[1], 3)
        self.assertEqual(points.dtype, np.float64)
        self.assertTrue(points.flags.c_contiguous)
        self.assertTrue(contour.closed)
        self.assertEqual(contour.metadata["ROIName"], "python contour")

        replacement = points + np.array([1.0, 2.0, 3.0])
        metadata = dict(contour.metadata)
        metadata["PythonRoundTrip"] = "preserved"
        session.data.set_contour(0, 0, replacement, False, metadata)
        replacement.fill(99.0)

        copied_back = session.data.get_contour(0, 0)
        np.testing.assert_array_equal(
            copied_back.to_numpy(),
            points + np.array([1.0, 2.0, 3.0]),
        )
        self.assertFalse(copied_back.closed)
        self.assertEqual(copied_back.metadata["PythonRoundTrip"], "preserved")
        self.assertEqual(session.data.contour_collection_count, 1)
        self.assertEqual(session.data.contour_count(0), 2)
        np.testing.assert_array_equal(contour.to_numpy(), points)

    def test_numpy_contour_copy_rejects_invalid_access_without_mutation(self):
        session = dcma.Session()
        session.run("GenerateSyntheticImages", NumberOfImages=1)
        session.run("ContourWholeImages")
        contour = session.data.get_contour(0, 0)
        original = contour.to_numpy()

        with self.assertRaises(IndexError):
            session.data.get_contour(1, 0)
        with self.assertRaises(IndexError):
            session.data.get_contour(0, 1)
        with self.assertRaisesRegex(ValueError, "point, xyz"):
            session.data.set_contour(0, 0, np.zeros((3, 2)), True, contour.metadata)

        copied_back = session.data.get_contour(0, 0)
        np.testing.assert_array_equal(copied_back.to_numpy(), original)
        self.assertEqual(copied_back.closed, contour.closed)
        self.assertEqual(copied_back.metadata, contour.metadata)

    def test_numpy_point_cloud_round_trip(self):
        session = dcma.Session()
        session.run("GenerateVirtualDataPointCloudV1")
        self.assertEqual(session.data.point_cloud_count, 1)

        point_cloud = session.data.get_point_cloud(0)
        points = point_cloud.points_to_numpy()
        normals = point_cloud.normals_to_numpy()
        colours = point_cloud.colours_to_numpy()
        self.assertEqual(points.shape, (100, 3))
        self.assertEqual(points.dtype, np.float64)
        self.assertEqual(normals.shape, (0, 3))
        self.assertEqual(colours.shape, (0,))
        self.assertEqual(colours.dtype, np.uint32)
        self.assertEqual(point_cloud.metadata["PointLabel"], "SyntheticCubeSample")

        replacement_points = points + np.array([1.0, 2.0, 3.0])
        replacement_normals = np.tile([0.0, 0.0, 1.0], (100, 1))
        replacement_colours = np.arange(100, dtype=np.uint32)
        metadata = dict(point_cloud.metadata)
        metadata["PythonRoundTrip"] = "preserved"
        session.data.set_point_cloud(
            0,
            replacement_points,
            replacement_normals,
            replacement_colours,
            metadata,
        )
        replacement_points.fill(99.0)
        replacement_normals.fill(99.0)
        replacement_colours.fill(99)

        copied_back = session.data.get_point_cloud(0)
        np.testing.assert_array_equal(
            copied_back.points_to_numpy(),
            points + np.array([1.0, 2.0, 3.0]),
        )
        np.testing.assert_array_equal(
            copied_back.normals_to_numpy(),
            np.tile([0.0, 0.0, 1.0], (100, 1)),
        )
        np.testing.assert_array_equal(copied_back.colours_to_numpy(), np.arange(100, dtype=np.uint32))
        self.assertEqual(copied_back.metadata["PythonRoundTrip"], "preserved")
        np.testing.assert_array_equal(point_cloud.points_to_numpy(), points)

    def test_numpy_point_cloud_copy_rejects_invalid_input_without_mutation(self):
        session = dcma.Session()
        session.run("GenerateVirtualDataPointCloudV1")
        point_cloud = session.data.get_point_cloud(0)
        original = point_cloud.points_to_numpy()

        with self.assertRaises(IndexError):
            session.data.get_point_cloud(1)
        with self.assertRaisesRegex(ValueError, "item, xyz"):
            session.data.set_point_cloud(
                0, np.zeros((3, 2)), np.empty((0, 3)), np.empty((0,), dtype=np.uint32), {}
            )
        with self.assertRaisesRegex(ValueError, "match the point count"):
            session.data.set_point_cloud(
                0, original, np.zeros((1, 3)), np.empty((0,), dtype=np.uint32), {}
            )

        np.testing.assert_array_equal(session.data.get_point_cloud(0).points_to_numpy(), original)

    def test_numpy_surface_mesh_round_trip(self):
        source_dir = Path(os.environ["DCMA_SOURCE_DIR"])
        fixture = source_dir / "artifacts/test_files/mesh_cube_with_normals.ply"
        session = dcma.Session()
        session.load([str(fixture)])
        self.assertEqual(session.data.surface_mesh_count, 1)

        mesh = session.data.get_surface_mesh(0)
        vertices = mesh.vertices_to_numpy()
        normals = mesh.normals_to_numpy()
        colours = mesh.colours_to_numpy()
        self.assertEqual(vertices.shape, (8, 3))
        self.assertEqual(vertices.dtype, np.float64)
        self.assertTrue(vertices.flags.c_contiguous)
        self.assertEqual(normals.shape, (8, 3))
        self.assertEqual(normals.dtype, np.float64)
        self.assertEqual(colours.shape, (0,))
        self.assertEqual(colours.dtype, np.uint32)
        self.assertEqual(len(mesh.faces), 12)
        self.assertEqual(mesh.faces[0], [2, 1, 0])
        self.assertEqual(mesh.metadata["MeshName"], "cube_corners_with_normals")

        replacement_vertices = vertices + np.array([1.0, 2.0, 3.0])
        replacement_normals = normals.copy()
        replacement_colours = np.arange(8, dtype=np.uint32)
        replacement_faces = list(reversed(mesh.faces))
        metadata = dict(mesh.metadata)
        metadata["PythonRoundTrip"] = "preserved"
        session.data.set_surface_mesh(
            0,
            replacement_vertices,
            replacement_normals,
            replacement_colours,
            replacement_faces,
            metadata,
        )
        replacement_vertices.fill(99.0)
        replacement_normals.fill(99.0)
        replacement_colours.fill(99)
        replacement_faces[0][0] = 0
        metadata["PythonRoundTrip"] = "changed"

        copied_back = session.data.get_surface_mesh(0)
        np.testing.assert_array_equal(
            copied_back.vertices_to_numpy(),
            vertices + np.array([1.0, 2.0, 3.0]),
        )
        np.testing.assert_array_equal(copied_back.normals_to_numpy(), normals)
        np.testing.assert_array_equal(copied_back.colours_to_numpy(), np.arange(8, dtype=np.uint32))
        self.assertEqual(copied_back.faces, list(reversed(mesh.faces)))
        self.assertEqual(copied_back.metadata["PythonRoundTrip"], "preserved")
        np.testing.assert_array_equal(mesh.vertices_to_numpy(), vertices)

        session.run("CopyMeshes", MeshSelection="last")
        self.assertEqual(session.data.surface_mesh_count, 2)
        copied_by_operation = session.data.get_surface_mesh(1)
        np.testing.assert_array_equal(copied_by_operation.vertices_to_numpy(), copied_back.vertices_to_numpy())
        self.assertEqual(copied_by_operation.faces, copied_back.faces)

    def test_numpy_surface_mesh_rejects_invalid_input_without_mutation(self):
        source_dir = Path(os.environ["DCMA_SOURCE_DIR"])
        fixture = source_dir / "artifacts/test_files/mesh_cube_with_normals.ply"
        session = dcma.Session()
        session.load([str(fixture)])
        mesh = session.data.get_surface_mesh(0)
        vertices = mesh.vertices_to_numpy()
        normals = mesh.normals_to_numpy()
        colours = mesh.colours_to_numpy()

        with self.assertRaises(IndexError):
            session.data.get_surface_mesh(1)
        with self.assertRaisesRegex(ValueError, "item, xyz"):
            session.data.set_surface_mesh(0, np.zeros((3, 2)), normals, colours, mesh.faces, {})
        with self.assertRaisesRegex(ValueError, "match the vertex count"):
            session.data.set_surface_mesh(0, vertices, np.zeros((1, 3)), colours, mesh.faces, {})
        with self.assertRaisesRegex(TypeError, "face indices"):
            session.data.set_surface_mesh(0, vertices, normals, colours, [[0, True, 2]], {})
        with self.assertRaisesRegex(ValueError, "non-negative"):
            session.data.set_surface_mesh(0, vertices, normals, colours, [[0, -1, 2]], {})
        with self.assertRaisesRegex(ValueError, "outside"):
            session.data.set_surface_mesh(0, vertices, normals, colours, [[0, 1, 8]], {})
        with self.assertRaisesRegex(ValueError, "at least one"):
            session.data.set_surface_mesh(0, vertices, normals, colours, [[]], {})

        copied_back = session.data.get_surface_mesh(0)
        np.testing.assert_array_equal(copied_back.vertices_to_numpy(), vertices)
        np.testing.assert_array_equal(copied_back.normals_to_numpy(), normals)
        np.testing.assert_array_equal(copied_back.colours_to_numpy(), colours)
        self.assertEqual(copied_back.faces, mesh.faces)
        self.assertEqual(copied_back.metadata, mesh.metadata)

    def test_rtplan_round_trip(self):
        session = self.load_rtplan_fixture()
        self.assertEqual(session.data.rtplan_count, 1)
        original = session.data.get_rtplan(0)
        self.assertGreater(len(original.dynamic_states), 0)
        self.assertGreater(len(original.dynamic_states[0].static_states), 0)

        dynamic_states = [dynamic_state_payload(state) for state in original.dynamic_states]
        metadata = dict(original.metadata)
        metadata["PythonRoundTrip"] = "plan"
        dynamic_states[0]["metadata"]["PythonRoundTrip"] = "beam"
        dynamic_states[0]["static_states"][0]["metadata"]["PythonRoundTrip"] = "control point"
        dynamic_states[0]["static_states"][0]["gantry_angle"] = 123.5
        dynamic_states[0]["static_states"][0]["jaw_positions_x"] = np.array([-7.0, 8.0])
        session.data.set_rtplan(0, dynamic_states, metadata)

        dynamic_states[0]["static_states"][0]["jaw_positions_x"].fill(99.0)
        metadata["PythonRoundTrip"] = "changed"
        copied_back = session.data.get_rtplan(0)
        first_beam = copied_back.dynamic_states[0]
        first_state = first_beam.static_states[0]
        self.assertEqual(copied_back.metadata["PythonRoundTrip"], "plan")
        self.assertEqual(first_beam.metadata["PythonRoundTrip"], "beam")
        self.assertEqual(first_state.metadata["PythonRoundTrip"], "control point")
        self.assertEqual(first_state.gantry_angle, 123.5)
        np.testing.assert_array_equal(first_state.jaw_positions_x, [-7.0, 8.0])
        self.assertNotIn("PythonRoundTrip", original.metadata)

        session.run("CopyRTPlans", RTPlanSelection="last")
        self.assertEqual(session.data.rtplan_count, 2)
        native_copy = session.data.get_rtplan(1)
        self.assertEqual(native_copy.metadata, copied_back.metadata)
        self.assertEqual(native_copy.dynamic_states[0].beam_number, first_beam.beam_number)
        np.testing.assert_array_equal(
            native_copy.dynamic_states[0].static_states[0].jaw_positions_x,
            first_state.jaw_positions_x,
        )

    def test_rtplan_rejects_invalid_input_without_mutation(self):
        session = self.load_rtplan_fixture()
        original = session.data.get_rtplan(0)
        original_states = [dynamic_state_payload(state) for state in original.dynamic_states]

        with self.assertRaises(IndexError):
            session.data.get_rtplan(1)

        missing_field = [dynamic_state_payload(state) for state in original.dynamic_states]
        del missing_field[0]["static_states"][0]["gantry_angle"]
        with self.assertRaisesRegex(KeyError, "gantry_angle"):
            session.data.set_rtplan(0, missing_field, original.metadata)

        boolean_index = [dynamic_state_payload(state) for state in original.dynamic_states]
        boolean_index[0]["static_states"][0]["control_point_index"] = True
        with self.assertRaisesRegex(TypeError, "control_point_index"):
            session.data.set_rtplan(0, boolean_index, original.metadata)

        invalid_position = [dynamic_state_payload(state) for state in original.dynamic_states]
        invalid_position[0]["static_states"][0]["isocentre_position"] = [1.0, 2.0]
        with self.assertRaisesRegex(ValueError, "xyz"):
            session.data.set_rtplan(0, invalid_position, original.metadata)

        copied_back = session.data.get_rtplan(0)
        self.assertEqual(copied_back.metadata, original.metadata)
        self.assertEqual(copied_back.dynamic_states[0].beam_number, original_states[0]["beam_number"])
        np.testing.assert_equal(
            copied_back.dynamic_states[0].static_states[0].jaw_positions_x,
            original_states[0]["static_states"][0]["jaw_positions_x"],
        )

    def test_numpy_line_sample_round_trip(self):
        session = dcma.Session()
        session.run("GenerateVirtualDataLineSampleV1")
        self.assertEqual(session.data.line_sample_count, 1)

        line_sample = session.data.get_line_sample(0)
        samples = line_sample.to_numpy()
        self.assertEqual(samples.shape, (150, 4))
        self.assertEqual(samples.dtype, np.float64)
        self.assertTrue(samples.flags.c_contiguous)
        self.assertFalse(line_sample.uncertainties_independent)
        self.assertEqual(line_sample.metadata["LineName"], "GaussianDistribution")

        replacement = samples.copy()
        replacement[:, 2] *= 2.0
        metadata = dict(line_sample.metadata)
        metadata["PythonRoundTrip"] = "preserved"
        session.data.set_line_sample(0, replacement, True, metadata)
        replacement.fill(99.0)

        copied_back = session.data.get_line_sample(0)
        expected = samples.copy()
        expected[:, 2] *= 2.0
        np.testing.assert_array_equal(copied_back.to_numpy(), expected)
        self.assertTrue(copied_back.uncertainties_independent)
        self.assertEqual(copied_back.metadata["PythonRoundTrip"], "preserved")
        np.testing.assert_array_equal(line_sample.to_numpy(), samples)

    def test_numpy_line_sample_copy_rejects_invalid_input_without_mutation(self):
        session = dcma.Session()
        session.run("GenerateVirtualDataLineSampleV1")
        line_sample = session.data.get_line_sample(0)
        original = line_sample.to_numpy()

        with self.assertRaises(IndexError):
            session.data.get_line_sample(1)
        with self.assertRaisesRegex(ValueError, "sample, "):
            session.data.set_line_sample(0, np.zeros((3, 3)), True, {})

        np.testing.assert_array_equal(session.data.get_line_sample(0).to_numpy(), original)

    def test_sparse_table_round_trip(self):
        session = dcma.Session()
        session.run("GenerateTable", TableLabel="python table")
        self.assertEqual(session.data.table_count, 1)

        original = session.data.get_table(0)
        self.assertEqual(original.cells, [])
        self.assertEqual(original.metadata["TableLabel"], "python table")

        cells = [(4, 8, "last"), (-2, 3, "negative"), (0, 0, "")]
        metadata = dict(original.metadata)
        metadata["PythonRoundTrip"] = "preserved"
        session.data.set_table(0, cells, metadata)
        cells[0] = (99, 99, "changed")
        metadata["PythonRoundTrip"] = "changed"

        copied_back = session.data.get_table(0)
        self.assertEqual(
            copied_back.cells,
            [(-2, 3, "negative"), (0, 0, ""), (4, 8, "last")],
        )
        self.assertEqual(copied_back.metadata["PythonRoundTrip"], "preserved")
        self.assertEqual(original.cells, [])

        session.run("CopyTables", TableSelection="last")
        self.assertEqual(session.data.table_count, 2)
        self.assertEqual(session.data.get_table(1).cells, copied_back.cells)

    def test_sparse_table_rejects_invalid_input_without_mutation(self):
        session = dcma.Session()
        session.run("GenerateTable")
        session.data.set_table(0, [(0, 0, "value")], {"key": "value"})
        original = session.data.get_table(0)

        with self.assertRaises(IndexError):
            session.data.get_table(1)
        with self.assertRaisesRegex(TypeError, "table cell"):
            session.data.set_table(0, [[0, 0, "value"]], {})
        with self.assertRaisesRegex(TypeError, "table cell"):
            session.data.set_table(0, [(True, 0, "value")], {})
        with self.assertRaisesRegex(TypeError, "table cell"):
            session.data.set_table(0, [(0, 0, 1)], {})
        with self.assertRaisesRegex(ValueError, "duplicate"):
            session.data.set_table(0, [(0, 0, "a"), (0, 0, "b")], {})

        copied_back = session.data.get_table(0)
        self.assertEqual(copied_back.cells, original.cells)
        self.assertEqual(copied_back.metadata, original.metadata)

    def test_affine_transform_round_trip(self):
        session = dcma.Session()
        session.run(
            "GenerateWarp",
            Transforms="translate(1.0, -2.0, 3.0); scale(0.0, 0.0, 0.0, 2.0)",
            TransformName="python affine",
            Metadata="PythonRoundTrip@source",
        )
        self.assertEqual(session.data.transform_count, 1)
        original = session.data.get_transform(0)
        self.assertEqual(original.kind, "affine")
        matrix = original.payload["matrix"]
        self.assertEqual(matrix.shape, (4, 4))
        self.assertEqual(matrix.dtype, np.float64)
        self.assertTrue(matrix.flags.c_contiguous)
        np.testing.assert_array_equal(matrix[3], [0.0, 0.0, 0.0, 1.0])

        replacement = matrix.copy()
        replacement[0, 3] = 17.5
        metadata = dict(original.metadata)
        metadata["PythonRoundTrip"] = "preserved"
        session.data.set_transform(0, "affine", {"matrix": replacement}, metadata)
        replacement.fill(99.0)
        metadata["PythonRoundTrip"] = "changed"

        copied_back = session.data.get_transform(0)
        self.assertEqual(copied_back.payload["matrix"][0, 3], 17.5)
        np.testing.assert_array_equal(copied_back.payload["matrix"][3], [0.0, 0.0, 0.0, 1.0])
        self.assertEqual(copied_back.metadata["PythonRoundTrip"], "preserved")
        np.testing.assert_array_equal(original.payload["matrix"], matrix)

        session.run("CopyWarps", TransformSelection="last")
        self.assertEqual(session.data.transform_count, 2)
        native_copy = session.data.get_transform(1)
        np.testing.assert_array_equal(native_copy.payload["matrix"], copied_back.payload["matrix"])
        self.assertEqual(native_copy.metadata, copied_back.metadata)

    def test_transform_rejects_invalid_affine_without_mutation(self):
        session = dcma.Session()
        session.run("GenerateWarp")
        original = session.data.get_transform(0)
        matrix = original.payload["matrix"]

        with self.assertRaises(IndexError):
            session.data.get_transform(1)
        with self.assertRaisesRegex(ValueError, "transform kind"):
            session.data.set_transform(0, "unknown", {}, {})
        with self.assertRaisesRegex(ValueError, "shape"):
            session.data.set_transform(0, "affine", {"matrix": np.eye(3)}, {})
        invalid_bottom_row = matrix.copy()
        invalid_bottom_row[3, 0] = 1.0
        with self.assertRaisesRegex(ValueError, "bottom row"):
            session.data.set_transform(0, "affine", {"matrix": invalid_bottom_row}, {})

        copied_back = session.data.get_transform(0)
        self.assertEqual(copied_back.kind, original.kind)
        self.assertEqual(copied_back.metadata, original.metadata)
        np.testing.assert_array_equal(copied_back.payload["matrix"], matrix)

    def test_thin_plate_spline_transform_round_trip(self):
        session = dcma.Session()
        session.run("GenerateWarp")
        points = np.array([
            [0.0, 0.0, 0.0],
            [1.0, 0.0, 0.0],
            [0.0, 1.0, 0.0],
            [0.0, 0.0, 1.0],
        ])
        normals = np.tile([0.0, 0.0, 1.0], (4, 1))
        colours = np.arange(4, dtype=np.uint32)
        coefficients = np.zeros((8, 3))
        coefficients[4] = [1.0, 2.0, 3.0]
        coefficients[5:, :] = np.eye(3)
        payload = {
            "control_points": points,
            "control_point_normals": normals,
            "control_point_colours": colours,
            "control_point_metadata": {"source": "python"},
            "kernel_dimension": 2,
            "coefficients": coefficients,
        }
        session.data.set_transform(0, "thin_plate_spline", payload, {"name": "tps"})
        points.fill(99.0)
        normals.fill(99.0)
        colours.fill(99)
        coefficients.fill(99.0)

        copied_back = session.data.get_transform(0)
        self.assertEqual(copied_back.kind, "thin_plate_spline")
        self.assertEqual(copied_back.metadata, {"name": "tps"})
        self.assertEqual(copied_back.payload["kernel_dimension"], 2)
        np.testing.assert_array_equal(copied_back.payload["control_points"], [
            [0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]
        ])
        np.testing.assert_array_equal(copied_back.payload["control_point_normals"],
                                      np.tile([0.0, 0.0, 1.0], (4, 1)))
        np.testing.assert_array_equal(copied_back.payload["control_point_colours"], np.arange(4))
        self.assertEqual(copied_back.payload["control_point_metadata"], {"source": "python"})
        expected_coefficients = np.zeros((8, 3))
        expected_coefficients[4] = [1.0, 2.0, 3.0]
        expected_coefficients[5:, :] = np.eye(3)
        np.testing.assert_array_equal(copied_back.payload["coefficients"], expected_coefficients)

        invalid = dict(copied_back.payload)
        invalid["coefficients"] = np.zeros((7, 3))
        with self.assertRaisesRegex(ValueError, r"control_points \+ 4"):
            session.data.set_transform(0, "thin_plate_spline", invalid, {})
        self.assertEqual(session.data.get_transform(0).metadata, {"name": "tps"})

        session.run("CopyWarps", TransformSelection="last")
        self.assertEqual(session.data.get_transform(1).kind, "thin_plate_spline")
        np.testing.assert_array_equal(
            session.data.get_transform(1).payload["coefficients"], expected_coefficients
        )

    def test_deformation_field_and_disengaged_transform_round_trip(self):
        session = dcma.Session()
        session.run("GenerateWarp")
        expected_values = np.arange(36, dtype=np.float64).reshape((3, 4, 3))
        values = expected_values.copy()
        image = {
            "values": values,
            "spacing": (1.5, 2.5, 3.5),
            "anchor": (10.0, 20.0, 30.0),
            "offset": (1.0, 2.0, 3.0),
            "row_direction": (1.0, 0.0, 0.0),
            "column_direction": (0.0, 1.0, 0.0),
            "metadata": {"plane": "one"},
        }
        session.data.set_transform(
            0, "deformation_field", {"images": [image]}, {"name": "field"}
        )
        values.fill(99.0)
        image["metadata"]["plane"] = "changed"

        field = session.data.get_transform(0)
        self.assertEqual(field.kind, "deformation_field")
        self.assertEqual(field.metadata, {"name": "field"})
        self.assertEqual(len(field.payload["images"]), 1)
        plane = field.payload["images"][0]
        self.assertEqual(plane["values"].shape, (3, 4, 3))
        np.testing.assert_array_equal(plane["values"], expected_values)
        self.assertEqual(plane["spacing"], (1.5, 2.5, 3.5))
        self.assertEqual(plane["anchor"], (10.0, 20.0, 30.0))
        self.assertEqual(plane["offset"], (1.0, 2.0, 3.0))
        self.assertEqual(plane["row_direction"], (1.0, 0.0, 0.0))
        self.assertEqual(plane["column_direction"], (0.0, 1.0, 0.0))
        self.assertEqual(plane["metadata"], {"plane": "one"})

        session.run("CopyWarps", TransformSelection="last")
        copied_field = session.data.get_transform(1)
        np.testing.assert_array_equal(
            copied_field.payload["images"][0]["values"], plane["values"]
        )
        with self.assertRaisesRegex(ValueError, "No images provided"):
            session.data.set_transform(1, "deformation_field", {"images": []}, {})
        self.assertEqual(session.data.get_transform(1).metadata, {"name": "field"})

        snapshot = copied_field
        session.data.set_transform(1, "disengaged", {}, {"state": "empty"})
        disengaged = session.data.get_transform(1)
        self.assertEqual(disengaged.kind, "disengaged")
        self.assertEqual(disengaged.payload, {})
        self.assertEqual(disengaged.metadata, {"state": "empty"})
        self.assertEqual(snapshot.kind, "deformation_field")
        np.testing.assert_array_equal(snapshot.payload["images"][0]["values"], plane["values"])

    def test_deterministic_operation_matches_cli(self):
        dispatcher_name = os.environ.get("DCMA_BIN")
        if not dispatcher_name:
            self.skipTest("DCMA_BIN is not set; native CLI parity test is unavailable")
        dispatcher = Path(dispatcher_name)
        with tempfile.TemporaryDirectory() as directory:
            python_output = Path(directory) / "python.csv"
            cli_output = Path(directory) / "cli.csv"

            session = dcma.Session()
            session.set_metadata("Parity", "fixed-value")
            session.run(
                "ConvertParametersToTable",
                KeySelection="^Parity$",
                TableLabel="parity",
                EmitHeader=True,
                Shape="wide",
            )
            session.run("ExportTables", Filename=str(python_output))

            result = subprocess.run(
                [
                    str(dispatcher),
                    "-v",
                    "-m", "Parity=fixed-value",
                    "-o", "ConvertParametersToTable",
                    "-p", "KeySelection=^Parity$",
                    "-p", "TableLabel=parity",
                    "-p", "EmitHeader=true",
                    "-p", "Shape=wide",
                    "-o", "ExportTables",
                    "-p", f"Filename={cli_output}",
                ],
                capture_output=True,
                text=True,
            )
            self.assertTrue(cli_output.exists(), result.stderr)
            self.assertEqual(python_output.read_bytes(), cli_output.read_bytes())


if __name__ == "__main__":
    unittest.main()
