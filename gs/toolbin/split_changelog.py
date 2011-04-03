#!/usr/bin/env python

#    Copyright (C) 2003-2007 Artifex Software, Inc.  All rights reserved.
# 
# This software is provided AS-IS with no warranty, either express or
# implied.
# 
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
# 
# For more information about licensing, please refer to
# http://www.ghostscript.com/licensing/. For information on
# commercial licensing, go to http://www.artifex.com/licensing/ or
# contact Artifex Software, Inc., 101 Lucas Valley Road #110,
# San Rafael, CA  94903, U.S.A., +1(415)492-9861.

# $Id$

# script to generate the split Changes/Details html changelogs
# for Ghostscript from the output of 'svn log --xml'

import string, re
import xml.parsers.expat
import sys, codecs, os

def html_escape(string):
  table = { '&': '&amp;',
            '"': '&quot;',
            '>': '&gt;',
            '<': '&lt;'}
  new = []
  for char in string:
    new.append(table.get(char,char))
  return "".join(new)

class Entry:
	'''a class representing a single changelog entry'''
	data = {}
	has_details = False
	has_differences = False
	r = re.compile('^[\[ ]*DETAILS[ :\]]*$', re.I)
	c = re.compile('^[ ]*EXPECTED DIFFERENCES[ :]*$', re.I)
	d = re.compile('^[ ]*DIFFERENCES[ :]*$', re.I)
	codec = codecs.getencoder("utf8") 
	def reset(self):
		self.data = {}
		self.has_details = False
		self.has_differences = False
	def add(self, key, value):
		if not self.data.has_key(key): self.data[key] = value
		else: self.data[key] = string.join((self.data[key], value))
	def listadd(self, key, value):
		if not self.data.has_key(key): self.data[key] = [value]
		else: self.data[key].append(value)
	def addmsg(self, value):
		if not self.data.has_key('msg'): self.data['msg'] = []
		self.data['msg'].append(value)
		if self.r.search(value): self.has_details = True
		if self.c.search(value): self.has_differences = True
		if self.d.search(value): self.has_differences = True
	def write(self, file, details=True):
		#stamp = self.data['date'] + ' ' + self.data['time']
		stamp = self.data['date']
		# construct the name anchor
		label = ''
		for c in stamp:
			if c == ' ': c = '_'
			if c == ':': continue
			label = label + c
		file.write('\n<p><strong>')
		file.write('<a name="' + label + '">')
		file.write('</a>\n')
		if self.data['author'] in authors:
		  self.data['author'] = authors[self.data['author']]
		file.write(stamp + ' ' + self.data['author'])
		file.write('</strong>')
		if not details and self.has_details:
			file.write(' (<a href="' + details_fn + '#' + label + '">details</a>)')
		file.write('</p>\n')
		file.write('<blockquote>\n')
		file.write('<pre>\n')
		try:
		  for line in self.data['msg']:
			# skip the details unless wanted
			if not details and self.r.search(line): break
			if self.c.search(line): break
			if self.d.search(line): break
			file.write(html_escape(line.encode('utf8')))
		except KeyError:
		  line = '(empty)'
		  file.write(line.encode('utf8'))
		file.write('</pre>\n')
		file.write('<p>[')
		#file.write(string.join(map(string.join, zip(self.data['name'],self.data['revision'])),', '))
		#file.write(string.join(self.data['name']))
		file.write(string.join(self.data['path']))
		file.write(']</p>\n')
		file.write('</blockquote>\n')

def write_header(file, details=True):
	file.write('<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"')
	file.write('   "http://www.w3.org/TR/html4/strict.dtd">\n')
	file.write('<html>\n')
	file.write('<head>\n')
	file.write('<title>')
	file.write('Ghostscript change history')
	if details:
		file.write(' (detailed)</title>')
	file.write('</title>\n')
	file.write('<!-- generated by split_changelog.py from the output of cvs2cl.pl -->\n')
	file.write('<!-- $Id$ -->\n')
	file.write('<link rel=stylesheet type="text/css" href="gs.css">\n')
	file.write('</head>\n')
	file.write('<body>\n')
	
def write_footer(file, details=True):
	file.write('</body>\n')
	file.write('</html>\n')

# expat hander functions
def start_element(name, attrs):
	#print 'Start element:', name, attrs
	element.append(name)
	if name == 'logentry': e = Entry()
def end_element(name):
	#print 'End element:', name
	element.pop()
	if name == 'logentry':
		e.write(details, True)
		e.write(changes, False)
		e.reset()
def char_data(data):
	if element[-1] == 'msg':
		# whitespace is meaningful inside the msg tag
		# so treat it specially
		e.addmsg(data)
	elif element[-1] == 'name' or element[-1] == 'path':
		# keep an ordered list of these elements
		item = string.strip(data)
		# hack off the prefix for mainline paths
		if item[:10] == '/trunk/gs/': item = item[10:]
		e.listadd(element[-1], item)
	else:
		data = string.strip(data)
		if data:
			#print 'Character data:', data
			e.add(element[-1], data)
    

# global parsing state
element = []
e = Entry()

# create and hook up the expat instance
p = xml.parsers.expat.ParserCreate()
p.StartElementHandler = start_element
p.EndElementHandler = end_element
p.CharacterDataHandler = char_data

# open our files
if len(sys.argv) != 4:
	print 'usage: convert the output of svn log -v --xml to html'
	print sys.argv[0] + ' <input xml> <output abbr> <output detailed>'
	sys.exit(2)
		
input = file(sys.argv[1], "r")
changes_fn = sys.argv[2]
details_fn = sys.argv[3]

changes = file(changes_fn, "wb")
details = file(details_fn, "wb")

# Try to parse out authors map
authors = {}
if os.path.exists('AUTHORS'):
  try:
  	a = open('AUTHORS', 'r')
  	for line in a.readlines():
  	  b = line.split(':')
  	  authors[b[0].strip()] = b[1].strip()
  except:
  	print "Error loading AUTHORS dictionary."

# read the input xml and write the output html
write_header(changes, False)
write_header(details, True)

while 1:
	line = input.readline()
	if line == '': break
	p.Parse(line)

input.close()

write_footer(changes, False)
write_footer(details, True)

# finished
changes.close()
details.close()
