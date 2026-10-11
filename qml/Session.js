.pragma library
// Sitzungsweite Flagge (geteilt über alle Importe, zurückgesetzt bei App-Neustart):
// die Auto-Anmeldung aus dem Schlüsselbund soll nur EINMAL je App-Start laufen, sonst
// würde „Abmelden" sofort wieder auto-anmelden (Endlosschleife).
var autoLoginDone = false
