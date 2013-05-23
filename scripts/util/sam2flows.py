#!/usr/bin/env python
# Copyright (C) 2013 Ion Torrent Systems, Inc. All Rights Reserved

import optparse
import random
import sys

def reverse_compliment(seq):
    c = {'a':'t', 'c':'g', 'g':'c', 't':'a', 'n':'n',
            'A':'T', 'C':'G', 'G':'C', 'T':'A', 'N':'N'}
    return "".join([c.get(nt, '') for nt in seq[::-1]])

def make_flow_signal(signal, sigma): 
    r = random.gauss(0, sigma)
    if 0 == signal:
        while r < 0 or 0.5 < r:
            r = random.gauss(0, sigma)
    else: 
        while r < -0.5 or 0.5 < r:
            r = random.gauss(0, sigma)
    return (100 * signal) + int(r * 100)

def check_option(parser, value, name):
    if None == value:
        print 'Option ' + name + ' required.\n'
        parser.print_help()
        sys.exit(1)

def __main__():
    parser = optparse.OptionParser()

    parser.add_option( '-f', '--flow-order', dest='flow_order', help='the flow order to simulate' )
    parser.add_option( '-k', '--key-sequence', dest='key_sequence', help='the key sequence to simulate' )
    parser.add_option( '-s', '--sam-file', dest='sam_file', help='the input SAM file' )
    parser.add_option( '-S', '--seed', dest='seed', help='the seed for the random number generator' )
    parser.add_option( '-d', '--sigma', dest='sigma', default=0.25, type=float, help='the standard deviation for the flow signal' )

    (options, args) = parser.parse_args()
    
    if len(args) != 0:
        parser.print_help()
        sys.exit(1)
    check_option(parser, options.flow_order, '-f')
    check_option(parser, options.key_sequence, '-k')
    check_option(parser, options.sam_file, '-s')

    if None != options.seed:
        random.seed(options.seed)

    flow_order = options.flow_order.upper()
    key_sequence = options.key_sequence.upper()
    key_sequence_len = len(options.key_sequence)

    rg_set = 0
    fp = open(options.sam_file, 'r')
    for line in fp:
        line = line.rstrip('\r\n')
        if line[0] == '@':
            if rg_set < 0:
                raise( 'RG was already set' )
            if line[0:3] == '@RG':
                line = line + ('\tFO:%s\tKS:%s' % (flow_order, options.key_sequence)) 
                rg_set = 1
            sys.stdout.write(line + '\n')
            continue
        elif 0 == rg_set:
            sys.stdout.write('@RG\tID:XYZ\tSM:SM\tFO:%s\tKS:%s\n' % (flow_order, options.key_sequence))
        if 0 == rg_set:
            rg_set = -1 # dummy
        elif 0 < rg_set:
            rg_set = -2 # add to previous
        if -1 == rg_set:
            sys.stdout.write(line + '\tRG:Z:XYZ\tFZ:B:S')
        else:
            sys.stdout.write(line + '\tFZ:B:S')
        tokens = line.split('\t')
        seq = tokens[9].upper()
        if tokens[1] == '16':
            seq = reverse_compliment(seq)
        elif tokens[1] != '4' and tokens[1] != '0':
            raise( 'unknown flag' )
        seq = key_sequence + seq
        zf = 0
        i = j = k = 0;
        while i < len(seq):
            # skip non-incorporating flows
            while seq[i] != flow_order[j]:
                sys.stdout.write(',' + str(make_flow_signal(0, options.sigma)))
                j = (j + 1) % len(flow_order)
                k += 1
            # for the ZF tag
            if i <= key_sequence_len:
                zf = k;
            # get hp length
            l = 1
            while i < len(seq)-1 and seq[i] == seq[i+1]:
                l += 1
                i += 1
            # write flow signal
            sys.stdout.write(',' + str(make_flow_signal(l, options.sigma)))
            # next flow, next base
            j = (j + 1) % len(flow_order)
            k += 1
            i += 1
        sys.stdout.write('\tZF:i:%s' % zf)
        sys.stdout.write('\n')
    fp.close()

if __name__ == "__main__":
    __main__()
