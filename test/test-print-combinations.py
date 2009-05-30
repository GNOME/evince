#!/usr/bin/python

# Test various print setting combinations
# To run this successfully you need to make some preparations. First of all,
# you need to open evince and make sure that:
#  * the printer is set to "Print to File"
# (I think) we are unable to select the "Print to File" line from the table

import os
# i'm note sure why this is required but it seems to break i18n if it is not commented out
# os.environ['LANG']='C'
homedir = os.environ["HOME"] + "/";

from dogtail.procedural import *
import dogtail.tree
import dogtail.predicate

#~ uncommenting the import and load below should enable you to run this on any language
#~ If you are testing a different language than English run the test like so:
#~ LANG=xx_XX.YYY ./test-print-combinations.py

# import dogtail.i18n
# dogtail.i18n.loadTranslationsFromPackageMoFiles('evince')

#~ test setting lists: customize these to your liking, the comment above each
#~ displays possible AND/OR default values

#~ copies = [1,2,3,4,....,n]  # does not need to be sequential
copies = [3,1]

#~ collate [0,1]
collate = [1,0]

#~ reverse = [0,1]
reverse = [1,0]

#~ pages_per_sheet = [1,2,4,6,9,16]
pages_per_sheet = [1,4,9]

#~ only_print = ["All sheets","Even sheets","Odd sheets"]
only_print = ["All sheets","Even sheets","Odd sheets"]

#~ output_type = ["pdf","ps"]
output_type = ["pdf"]

#~ if you prepare more test documents with different numbers of pages, 
#~ you can add them here, the test documents must be saved as n-page.pdf
#~ where n stands for the number of pages

#~ pages_in_document = [3,4]
pages_in_document = [3,4]

# all pages in a document, an even range, an odd range
#~ ranges = ["all","1-3,2-3,1","1-2,2-3,1-3"]
ranges = ["all","1-3,2-3,1","1-2,2-3,1-3"]

counter = 0
# estimate number of iterations the test will require, this will be lower because we don't run (col == 1 && cop == 1)
n_tests = len(copies) * len(collate) * len(reverse) * len(pages_per_sheet) * len(only_print) * len(output_type) * len(pages_in_document) * len(ranges)

#~ function: build_filename
def build_filename( pages, cop, pps, col, rev, rng, op, ot ):
	result = "pid_" + str(pages) + "_cop_" + str(cop) + "_pps_" + str(pps) + "_col_" + str(col) + "_rev_" + str(rev)

	result += "_sheets"		
	if op == "All sheets":
		result += "_all"
	elif op == "Even sheets":
		result += "_even"
	else:
		result += "_odd"
	
	result += "_rng"	
	if rng == "all":
		result += "_all"
	elif rng == "1-3,2-3,1":
		result += "_even"
	else:
		result += "_odd"

	result += "."
	result += str(ot)
	return result
#~ ///function: build_filename



#~ function: run_test
def run_test( pages, cop, pps, col, rev, rng, op, ot ):
	filename = build_filename( pages, cop, pps, col, rev, rng, op, ot )

	#~ we don't want the "file exists" dialog to pop up:
	#~ delete the file if it already exists, potentially dangerous if filename
	#~ is zero so we check for nonzero length at least
	if ( os.path.exists(homedir + filename) and (len(filename) != 0) ):
		os.unlink(homedir + filename)

	evince = tree.root.application('evince')
	
	click('File', roleName='menu')
	click('Print...', roleName='menu item')
	
	dlg = evince.dialog('Print')
	
	#~ This doesn't work, even if rewritten as in test6.py or test7.py
	#~ click(name='Print to File', roleName='table cell', raw=True)
	
	focus.widget(roleName='page tab', name='General')
	focus.widget.node.select()
	
	dlg.child( roleName='text' ).text = filename
	
	if ot == "pdf":
		click('PDF', roleName='radio button')
	else:
		click('Postscript', roleName='radio button')
		
	
	if rng == "all":
		click('All Pages', roleName='radio button')
	else:
		click('Pages:', roleName='radio button')
		dlg.child('Pages', roleName='text').text = rng
	
	
	sb_copies = dlg.child( roleName='spin button' )
	sb_copies.text = str(cop)
	# activate must be called to actualize the new setting
	sb_copies.doAction("activate")
	
	
	cb_reverse = dlg.child('Reverse', roleName='check box')
	if (rev == 1 and cb_reverse.checked == False):
		cb_reverse.click()
	elif (rev == 0 and cb_reverse.checked == True):
		cb_reverse.click()
	
	cb_collate = dlg.child('Collate', roleName='check box')
	if (col == 1 and cb_collate.checked == False):
		cb_collate.click()
	elif (col == 0 and cb_collate.checked == True):
		cb_collate.click()
		
	focus.widget(roleName='page tab', name='Page Setup')
	focus.widget.node.select()
	
	#~ Set pages per sheet:
	#~ This will break at some point if there is more than one numeric combo box
	#~ it will also not work for languages with other numerals
	#~ The for loops here are necessary because the name of these combo boxes
	#~ is based on their current value and they have no 'label' attribute
	
	# Redefining this here so that changing pages_per_sheet above does not affect our search
	local_pages_per_sheet = [1,2,4,6,9,16]
	for x in local_pages_per_sheet:
		pred = dogtail.predicate.GenericPredicate( name = str(x), roleName='combo box' )
		if dlg.findChild( pred, retry=False, requireResult=False):
			# dlg.child( str(x), roleName='combo box').combovalue = str(pps)
			# we've found what we're looking for, no need to continue the loop
			break
	
	# Redefining this here so that changing only_print above does not affect our search
	local_only_print = ["All sheets","Even sheets","Odd sheets"]
	for x in local_only_print:	
		pred = dogtail.predicate.GenericPredicate( name = str(x), roleName='combo box' )
		if dlg.findChild( pred, retry=False, requireResult=False):
			dlg.child( str(x), roleName='combo box').combovalue = str(op)
			# we've found what we're looking for, no need to continue the loop
			break
	
	#~ We're done setting up, start the print job 
	click('Print', roleName='push button')
#~ ///function: run_test




#~ The test loop. 
for ot in output_type:
	for pages in pages_in_document:
		# the filename which is opened depends on 'pages'
		app = run('evince', arguments=' ' + str(pages) + "-page.pdf")
		for rev in reverse:
			for col in collate:
				for cop in copies:
					#no sense in testing collate with 1 copy! it is equivalent to 1 uncollated
					if (cop == 1 and col == 1):
						continue
					for rng in ranges:
						for pps in pages_per_sheet:
							for op in only_print:
								counter += 1
								print str(counter) + " of " + str(n_tests)
								run_test( pages, cop, pps, col, rev, rng, op, ot )
		click('File',roleName='menu')
		click('Close',roleName='menu item')

#~ we should be done now.

