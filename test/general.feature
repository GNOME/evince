Feature: General actions

  @start_via_command
  Scenario: Start via command
    * Start Evince via command
    Then evince should start

  @start_via_menu
  Scenario: Start via menu
    * Start Evince via menu
    Then evince should start

  @quit_via_shortcut
  Scenario: Ctrl-Q to quit application
    * Start Evince via command
    * Press "<Ctrl>q"
    Then evince shouldn't be running anymore

  @close_via_gnome_panel
  Scenario: Close via menu
    * Start Evince via menu
    * Click "Quit" in GApplication menu
    Then Evince shouldn't be running anymore
