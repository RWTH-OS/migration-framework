

#OUTPUT=slides

.PHONY : default quick clean

default : slides.pdf

slides.pdf : slides.tex
	#$(MAKE) -C pictures
	lualatex slides
	lualatex slides


quick : 
	lualatex slides

clean :
	-rm -f *.aux *.log *.nav *.out *.snm *.toc *.vrb *.auxlock

