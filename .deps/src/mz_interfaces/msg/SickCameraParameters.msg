# SICK Ranger3 acquisition parameters.
# Published on /sick_camera/parameters and accepted on /sick_camera/set_parameters.

# Number of encoder-triggered lines in one complete scan image.
uint32 scan_lines

# Exposure time in microseconds; applied to Region0 and Region1 together.
float64 exposure_time_us

# SICK MultiSlopeMode. Supported values: Off, PresetMedium.
string hdr_mode

# Output Y resolution in millimeters: EncoderResolution * EncoderDivider.
float64 y_resolution_mm
