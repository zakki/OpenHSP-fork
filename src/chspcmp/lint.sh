#!/bin/sh

mkdir -p _report
cppcheck --xml --enable=warning,portability,unusedFunction . 2> _report/cppcheck.xml
PYTHONNOUSERSITE=1 cppcheck-htmlreport --file=_report/cppcheck.xml --report-dir=_report --source-dir=.
