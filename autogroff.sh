#!/bin/bash

trap cleanup exit;

cleanup() {
    for p in $(jobs -p); do
	echo "Killing $p"
	kill $p
    done
}    

for f in *.1; do
    file=${f%.1}
    groff -Tpdf -P-e -P-pletter -mpdfmark -mandoc $file.1 > $file.pdf
    echo "Waiting on $file.1.		HIT ^C TO CANCEL"
    while inotifywait -e MODIFY $file.1; do
	groff -Tpdf -P-e -P-pletter -mpdfmark -mandoc $file.1 > $file.pdf
	nroff -mandoc $file.1 > $file.cat
	echo "Waiting on $file.1.  HIT ^C TO CANCEL"
    done &
done

wait

    
