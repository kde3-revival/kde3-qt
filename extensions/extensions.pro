TEMPLATE = subdirs

contains( QT_PRODUCT, qt-(enterprise|internal|eval) ) {
	x11:	SUBDIRS = motif
}
