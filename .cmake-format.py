with section("format"):
  disable = True
  line_width = 120

with section("lint"):
  disabled_codes = [
      'C0103', # Invalid argument name
      'C0305', # Too many newlines between statements
      'C0307', # Bad indentation
  ]
