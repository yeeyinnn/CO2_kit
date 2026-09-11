function doGet(e) {

  var sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();

  sheet.appendRow([
    new Date(),
    e.parameter.temp || "",
    e.parameter.humidity || "",
    e.parameter.pressure || "",
    e.parameter.gas || "",
    e.parameter.co2 || "",
    e.parameter.vbat || ""
  ]);

  return ContentService.createTextOutput("OK");
}