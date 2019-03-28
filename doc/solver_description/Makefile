mergesat.pdf: always
	pdflatex mergesat
	pdflatex mergesat
	bibtex -terse mergesat
	pdflatex mergesat

always:

clean:
	rm -f *.pdf *.aux *.bbl *.log *.out *.tex.backup *.tex.bak *.blg
