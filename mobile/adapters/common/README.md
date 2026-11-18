# Numerical adapters

`generate_math.py` reads the current upstream `temperature.c`, `zonesystem.c`
and `colorspaces.h`. It writes only build-tree includes. No copy of their
spectral integration, camera multiplication, matrix constants or zone
interpolation is maintained here.

`whitebalance.c` calls those extracted original functions through small local
argument structs. These structs are private to the generated helper translation
unit; they never cross the library ABI or replace upstream structures. The
helper receives explicit matrices instead of a GTK module's GUI state. The
zone-map wrapper similarly supplies the original routine's size and control
points through a local argument struct.

The generator requires the reviewed function signatures, bounds and extraction
boundaries. A missing or changed contract fails generation. New function bodies
are used directly, subject to the integration's upstream-source review gate.
Generated output and every input must be CMake configure dependencies, so an
upstream update cannot retain old generated code.

The regular desktop build compiles the upstream modules directly. Only the
mobile integration compiles these adapters and the temporary module extension
includes.
