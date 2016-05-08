FotoScan
========

Tool for extracting photos in a scan or large picture.

FotoScan has been developed to facilitate the process of photo album
digitalization, starting from pictures (or more generally, scans) of the pages
of a photo album and resulting in individual pictures.

The tool features a three-step process:
- **Detection**: identify rectangle-like shapes with sharp edges
- **Review**: let the user correct the detected shapes (improve detected shapes,
  remove faulty ones, add missing ones)
- **Post-processing**: extract photos, and correct their perspective and
  orientation

**WARNING**: this tool has been developed with a **very** specific use-case in
mind, and hence is not ready for general use. The UI is functional and fast, but
lacking features and visual clues. Documentation is sparse. You will probably
need to adapt the code to suit your needs.



Building
--------

Requirements:

- Qt 5 (tested on 5.6.0)
- OpenCV 3 (tested on 3.1.0)

Optionally, but recommended: a compiler with OpenMP support. If unavailable,
task-based parallelism will be used, but this has not been tested thoroughly.

```
qmake
make -j5
```



Usage
-----

### Command-line interface

```
$ ./FotoScan --help
Usage: ./FotoScan [options] INPUT-DIRECTORY
Foto Scanner

Options:
  -h, --help                          Displays this help.
  -v, --version                       Displays version information.
  -o, --output-directory <directory>  Write photos to <directory>.
  -c, --correct                       Correct most recent results

Arguments:
  INPUT-DIRECTORY                     Path to scan for images.
```

Results of detection and review are saved as `.dat` files next to the source
images, so you can safely quit and re-start the application. Note that the
post-processing steps will be repeated for all images, including previously
post-processed ones.

Use the `--correct` option to re-review the results of previous detections,
starting with the most recently modified set of results.


### Graphical user interface

Shape colour code:

- green: detected shapes
- yellow: a pending shape being constructed
- blue: detected shapes before grouping (only shown when `Show Ungrouped` is
  checked)
- red: rejected shapes (only shown when `Show Rejected` is checked)

Controls:

- Left mouse-button drag to pan
- Scrollwheel to zoom
- Right mouse-button click to select (when clicking inside) and deselect (when
  clicking outside) shapes, or define a new shape when nothing is selected and
  not clicking inside another shape
- Right mouse-button drag to move points close by

While a shape is selected:
- ESCAPE to deselect
- DELETE to remove

While a shape is being constructed:
- ESCAPE/DELETE to cancel
- RETURN to confirm

Other situations:
- RETURN to move to the next scan



ToDo
----

### Exposure and color correction

Detect or provide a reference point in the scan to detect the exposure and/or
white balance, and correct all photos within a scan.
