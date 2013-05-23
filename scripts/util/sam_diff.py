#!/usr/bin/env python
# Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved

import os
import sys
import time
import argparse
import progressbar
from collections import deque

fields = ['qname', 'flag', 'rname', 'pos', 'mapq',
          'cigar', 'rnext', 'pnext', 'tlen', 'seq',
          'qual', 'rg', 'pg', 'md', 'nm', 'as', 'fz', 
          'xa', 'xs', 'xt']

class Record(object):
    unNamedRange = (0, 10)
    def __init__(self, tokens):
        self._set_attrs( tokens )
        """
        self.fields_len = len(tokens)
        for x in xrange(len(fields)):
            try:
                setattr(self, fields[x], tokens[x])
            except:
                print tokens
                print fields
                print "len(tokens)=", len(tokens)
                print "len(fields)=", len(fields)
                sys.exit(1)
        """
    def __str__(self):
        return '\t'.join( [ getattr(self,fields[x]) for x in xrange(self.fields_len) ] )

    def _set_attrs( self, tokens ):
        for x in xrange( self.unNamedRange[1] ):
            setattr(self, fields[x], tokens[x])

        for x in xrange( self.unNamedRange[1] + 1, len( tokens ) ):
            fieldName = tokens[x].split(':')[0].lower()
            setattr(self, fieldName, tokens[x])

    def remove_hard_clip(self):
        attr = getattr(self, 'cigar')
        found = True
        while found:
            found = False
            i = attr.find('H')
            if -1 != i:
                found = True
                j = i-1
                while 0 <= j and '0' <= attr[j] and attr[j] <= '9':
                    j = j - 1
                if j < 0:
                    attr = attr[i+1:len(attr)]
                elif i == len(attr)-1:
                    attr = attr[0:j+1]
                else:
                    attr = attr[0:j+1] + attr[i+1:len(attr)]
                break
        setattr(self, fields[5], attr) # cigar is field 5

class Sam(object):

    def __init__(self, sam, full_qname ):
        self.buf = deque()
        self.buf_l = 10000
        self.sam = sam
        self.record = None
        self.name = None
        self.fp = open( self.sam, "r" )
        self.full_qname = full_qname
        for line in self.fp:
            if line[0] == '@':
                # not supporting headers for now
                continue
            break
        self.get_next();

    def get_next(self):
        self.record = None;
        self.name = None
        if 0 == len(self.buf):
            ctr = 0
            for line in self.fp:
                line = line.rstrip();
                tokens = line.split('\t')
                self.buf.append(Record(tokens))
                ctr += 1
                if self.buf_l == ctr:
                    break
        if 0 < len(self.buf):
            self.record = self.buf.popleft()
            self.name = self._get_name( self.record.qname, self.full_qname )
        if None == self.record and None != self.fp:
            self.close()
        return self.record

    def close(self):
        self.fp.close();
        self.fp = None

    def _get_name( self, name, full_qname ):
        if full_qname:
            return name
        else:
            return ':'.join(name.split(':')[1:])

def diff_field(field1, field2):
    """returns true if field1 == field2"""
    return field1 == field2

def main(options):
    sam1 = Sam(options.sam1, options.full_qname)
    sam2 = Sam(options.sam2, options.full_qname)
    fields = options.fields
    #fields = options.fields.split(',')
    #widgets = [
    #            "diffing: ", 
    #            progressbar.Percentage(),
    #            progressbar.Bar()
    #            ]
    #pbar = progressbar.ProgressBar( widgets=widgets, maxval=len( sam1.records) ).start()
    #widgets = ['Processed: ', progressbar.Counter(), ' records (', progressbar.Timer(), ')']
    #pbar = progressbar.ProgressBar( widgets=widgets).start()

    sys.stderr.write("Processing")
    counter = 1
    while True:
        r1 = sam1.get_next()
        r2 = sam2.get_next()
        if None == r1 and None == r2:
            break
        elif None == r1 and None != r2:
            sys.stderr.write("Error: early EOF on input file #1.\n")
            sys.exit(1)
        elif None != r1 and None == r2:
            sys.stderr.write("Error: early EOF on input file #2.\n")
            sys.exit(1)

        if sam1.name != sam2.name: 
            sys.stderr.write("Error: read names do not match: [%s] != [%s].\n" % (sam1.name, sam2.name))
            sys.exit(1)

        if 0 < options.min_mapq:
            mapq1 = int(getattr( r1, 'mapq' ))
            mapq2 = int(getattr( r2, 'mapq' ))
            if mapq1 < options.min_mapq and mapq2 < options.min_mapq:
                continue

        if options.ignore_hard_clip:
            r1.remove_hard_clip()
            r2.remove_hard_clip()

        diff_str = "[%s]" % (sam1.name)
        for field in fields:
            cont = [False, False] #var to track these 2 exceptions below to see which one failed
            try:
                attr1 = getattr( r1, field )
            except:
                #print sam1.sam, sam1.name, "doesn't have the: ", field, " tag"
                cont[0] = True
            try:
                attr2 = getattr( r2, field )
            except:
                cont[1] = True
            if cont[0] and cont[1]:
                continue
            elif cont[0] and not cont[1]:
                print sam1.name, sam1.sam, "has the field: ", field, " and", sam2.sam, "does not"
                continue
            elif not cont[0] and cont[1]:
                print sam1.name, sam2.sam, "has the field: ", field, " and", sam1.sam, "does not"
                continue
            if not diff_field( attr1, attr2 ):
                diff_str = "%s -- %s[%s]=%s %s[%s]=%s" % (diff_str, sam1.sam, field, str(attr1), sam2.sam, field, str(attr2))
        if len(diff_str) > len(sam1.name) + 2:
            print diff_str
        #pbar.update(counter)
        if 0 == (counter % 10000):
            sys.stderr.write("\rProcessed %d records" % counter)
            sys.stderr.flush()
        counter = counter + 1
    sys.stderr.write("\rProcessed %d records\n" % counter)
    #pbar.finish()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Diff two SAM files")
    parser.add_argument('--sam1', help="first sam file to diff.  will be called sam1 in diff out", dest='sam1')
    parser.add_argument('--sam2', help="second sam file to diff.  will be called sam2 in diff out", dest='sam2')
    parser.add_argument('--fields',
                      help="comma seperated list of fields:%s\t\t\t\t\t to diff between the"
                      "sam records use names from the same spec. for optional tags"
                      "use their 2 letter abbreviation.  Default:  pos" % (str(fields)),
                      dest='fields', action="append", default=[])
    parser.add_argument('--full-qname', help="keep the full query name", dest='full_qname', action="store_true", default=False)
    parser.add_argument('--min-mapq', help="examine only those records with a given minimum mapping quality", type=int, dest="min_mapq", default=0)
    parser.add_argument('--ignore-hard-clip', help="ignore hard clips in the cigar", dest="ignore_hard_clip", action="store_true", default=False)
    options = parser.parse_args()
    if None == options.sam1:
        parser.print_help()
        sys.stderr.write( "Error: --sam1 not given\n");
        sys.exit(1);
    if None == options.sam2:
        parser.print_help()
        sys.stderr.write( "Error: --sam2 not given\n");
        sys.exit(1);
    for field in options.fields:
        if not field in fields:
            sys.stderr.write( "Error: field not recognized [%s]\n" % field)
            parser.print_help()
            sys.exit()
    main(options)
