%vcpkgRoot%/%VCPKG_DEFAULT_TRIPLET%/tools/flatbuffers/flatc --cpp -b --filename-suffix _bin fileformat.fbs
%vcpkgRoot%/%VCPKG_DEFAULT_TRIPLET%/tools/flatbuffers/flatc --cpp -t --filename-suffix _json fileformat.fbs
pause