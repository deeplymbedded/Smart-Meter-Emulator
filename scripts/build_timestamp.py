Import("env")

from datetime import datetime
import os

build_dir = env.subst("$BUILD_DIR")
generated_dir = os.path.join(build_dir, "generated")
os.makedirs(generated_dir, exist_ok=True)

build_timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
generated_header = os.path.join(generated_dir, "BuildInfoGenerated.h")
with open(generated_header, "w") as header:
    header.write(
        "#pragma once\n"
        '#define GATEWAY_BUILD_TIMESTAMP "' + build_timestamp + '"\n'
    )

env.Append(CPPPATH=[generated_dir])
