Feature: Basic UI Actions

  @test_file_open_button
  Scenario: Open new file
    * Make sure that Evince is running
    * Press "<Ctrl>o"
    Then Open document window should be open

  @test_document_viewer_settings
  Scenario: Edit viewer settings
    * Make sure that Evince is running
    * Open Document Viewer Settings

  @test_reload_doc
  Scenario: Check reloading
    * Make sure that Evince is running
    * Copy test document to ~/Documents
    * Press "<Ctrl>o"
    * Press "<enter>"
    * Press "<Ctrl>r"
    #Add the remove part in cleanup eventually
    * Remove test document from ~/Documents


  @test_print_file
  Scenario: Check Printing of Opened File
    * Make sure that Evince is running
    * Copy test document to ~/Documents
    * Press "<Ctrl>o"
    * Press "<enter>"
    * Press "<Ctrl>p"
    Then Print dialogue should appear
    * Remove test document from ~/Documents



#  @test_about_dialog_open
#  Scenario:Open About dialog
#    * Start Evince via command
#    * Make sure that Evince is running
#    * Open About Document Viewer
#    * Open Credits
#    Then Credits should show

#  @test_about_dialog_close
#  Scenario: Open About Document Viewer
#    * Open Credits
#    * Close credits
#    * Close About document viewer
#    Then About should not show
