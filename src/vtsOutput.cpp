#include <stdio.h>
#include <mpi.h>
#include "vtsOutput.h"
#include "vtkOutput.h"
#include <cstring>
#include <stdlib.h>

const char * vts_field_header = "<DataArray type=\"%s\" Name=\"%s\" format=\"binary\" encoding=\"base64\" NumberOfComponents=\"%d\">\n";
const char * vts_field_footer = "</DataArray>\n";
const char * vts_field_parallel = "<PDataArray type=\"%s\" Name=\"%s\" format=\"binary\" encoding=\"base64\" NumberOfComponents=\"%d\"/>\n";
const char * vts_footer       = "</Points>\n</Piece>\n</StructuredGrid>\n</VTKFile>\n";

// Error handler
#define FERR 	if (f == NULL) {fprintf(stderr, "Error: vtkOutput tried to write before opening a file\n"); return; } 


vtsFileOut::vtsFileOut(MPI_Comm comm_)
{
    f = NULL;
    fp = NULL;
    size = 0;
    comm = comm_;
};

int vtsFileOut::Open(const char* filename) {
    char* n;
    f = fopen(filename, "w");
    if(f == NULL) {
        fprintf(stderr, "Error, could not open vts file %s\n", filename);
        return -1;
    }
    int s = strlen(filename) + 5;
    name = new char[s];
    int rank;
    MPI_Comm_rank(comm, &rank);
    if(rank == 0) {
		strcpy(name, filename);
		n=name;
		while(*n != '\0') {
			if (strcmp(n, ".vts") == 0) break;
			n++;
		}
		strcpy(n, ".pvts");
		fp = fopen(name,"w");
		if (fp == NULL) {fprintf(stderr, "Error: Could not open (p)vtk file %s\n", name); return -1; }
	}
	n = name;
	while(*filename != '\0')
	{
		*n = *filename;
		if (*filename == '/') n = name; else n++;
		filename++;
	}
	*n = '\0';
	s = strlen(name)+1;
	MPI_Allreduce ( &s, &name_size, 1, MPI_INT, MPI_MAX, comm );
	return 0;
};

void vtsFileOut::WriteB64(void * tab, int len) {
	FERR;
	fprintB64(f, tab, len);
};

void vtsFileOut::Init(lbRegion regiontot, lbRegion region, size_t latticeSize, char* selection, double spacing) {
	FERR;
	size = latticeSize;
    fprintf(f, "<?xml version=\"1.0\"?>\n");
	fprintf(f, "<VTKFile type=\"StructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n");
	fprintf(f, "<StructuredGrid WholeExtent=\"%d %d %d %d %d %d\">\n",
		region.dx, region.dx + region.nx,
		region.dy, region.dy + region.ny,
		region.dz, region.dz + region.nz);

	fprintf(f, "<Piece Extent=\"%d %d %d %d %d %d\">\n",
		region.dx, region.dx + region.nx,
		region.dy, region.dy + region.ny,
		region.dz, region.dz + region.nz
	);
	fprintf(f, "<PointData %s>\n", selection);
	if (fp != NULL) {
        fprintf(fp, "<?xml version=\"1.0\"?>\n");
        fprintf(fp, "<VTKFile type=\"PStructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n");
        fprintf(fp, "<PStructuredGrid WholeExtent=\"%d %d %d %d %d %d\">\n",
            region.dx, region.dx + region.nx,
            region.dy, region.dy + region.ny,
            region.dz, region.dz + region.nz);
	}
	int size;
	lbRegion reg;
	MPI_Comm_size(comm, &size);
	char * buf = new char[name_size];
	for (int i=0;i<size;i++)
	{
		reg = region;
		MPI_Bcast(&reg, 6, MPI_INT, i, comm);
		strcpy(buf, name);
		MPI_Bcast(buf, name_size, MPI_CHAR, i, comm);
		if (fp != NULL) {
			fprintf(fp, "<Piece Extent=\"%d %d %d %d %d %d\" Source=\"%s\"/>\n",
				reg.dx, reg.dx + reg.nx,
				reg.dy, reg.dy + reg.ny,
				reg.dz, reg.dz + reg.nz,
				buf
			);
		}
	}
	delete[] buf;
	if (fp != NULL) {
		fprintf(fp, "<PPointData %s>\n", selection);
	}
};

void vtsFileOut::Init(lbRegion region, size_t latticeSize, char* selection) {
    Init(region, region, latticeSize, selection);
};

void vtsFileOut::Init(int width, int height, size_t latticeSize) {
    Init(lbRegion(0, 0, 0, width, height, 1), latticeSize, "");
};

void vtsFileOut::WriteField(const char * name, void * data, int elem, const char * tp, int components) {
	FERR;
	int len = size*elem;
	fprintf(f, vts_field_header, tp, name, components);
	WriteB64(&len, sizeof(int));
	WriteB64(data, size*elem);
	fprintf(f, "\n");
	fprintf(f, "%s", vts_field_footer);
	if (fp != NULL) {
		fprintf(fp, vts_field_parallel,  tp, name, components);
	}
};

void vtsFileOut::FinishCellData() {
	FERR;
	fprintf(f, "</PointData>\n");
	if (fp != NULL) {
		fprintf(fp, "</PPointData>\n");
	}
};

void vtsFileOut::WritePointsHeader() {
	FERR;
	fprintf(f, "<Points>\n");
	if(fp != NULL) {
		fprintf(fp, "<PPoints>\n");
	}
}

void vtsFileOut::Finish() {
	FERR;
	fprintf(f, "%s", vts_footer);
	if (fp != NULL) {
		fprintf(fp, "</PPoints>\n</PStructuredGrid>\n</VTKFile>\n");
	}
};

void vtsFileOut::Close() {
	FERR;
	fclose(f);
	if (fp != NULL) fclose(fp);
	f = NULL; size=0;
};