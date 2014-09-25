var Settings = {};

$(document).ready(function(){  
  /*
  * Clamped-width. 
  * Usage:
  *  <div data-clampedwidth=".myParent">This long content will force clamped width</div>
  *
  * Author: LV
  */
  
  $('[data-clampedwidth]').each(function () {
      var elem = $(this);
      var parentPanel = elem.data('clampedwidth');
      var resizeFn = function () {
          var sideBarNavWidth = $(parentPanel).width() - parseInt(elem.css('paddingLeft')) - parseInt(elem.css('paddingRight')) - parseInt(elem.css('marginLeft')) - parseInt(elem.css('marginRight')) - parseInt(elem.css('borderLeftWidth')) - parseInt(elem.css('borderRightWidth'));
          elem.css('width', sideBarNavWidth);
      };

      resizeFn();
      $(window).resize(resizeFn);
  });
  
  
  var panelsSource = $('#panels-template').html();
  Settings.panelsTemplate = _.template(panelsSource);
  
  var sidebarTemplate = $('#list-group-template').html();
  Settings.sidebarTemplate = _.template(sidebarTemplate)
  
  reloadSettings();
});

function reloadSettings() {
  $.getJSON('/settings.json', function(data){
    $('.list-group').html(Settings.sidebarTemplate(data))
    $('#panels').html(Settings.panelsTemplate(data));
  });
}

var SETTINGS_ERROR_MESSAGE = "There was a problem saving domain settings. Please try again!";

$('#settings').on('click', 'button', function(e){
  // disable any inputs not changed
  $("input:not([data-changed])").each(function(){
    $(this).prop('disabled', true);
  });
  
  // grab a JSON representation of the form via form2js
  var formJSON = form2js('settings-form', ".", false, cleanupFormValues, true);
  
  // re-enable all inputs
  $("input").each(function(){
    $(this).prop('disabled', false);
  });
  
  // POST the form JSON to the domain-server settings.json endpoint so the settings are saved
  $.ajax('/settings.json', {
    data: JSON.stringify(formJSON),
    contentType: 'application/json',
    type: 'POST'
  }).done(function(data){
    if (data.status == "success") {
      showAlertMessage("Domain settings saved.", true);
    } else {
      showAlertMessage(SETTINGS_ERROR_MESSAGE, false);
    }
    
    reloadSettings();
  }).fail(function(){
    showAlertMessage(SETTINGS_ERROR_MESSAGE, false);
    reloadSettings();
  });
  
  return false;
});

$('#settings').on('change', 'input', function(){
  // this input was changed, add the changed data attribute to it
  $(this).attr('data-changed', true);
});

function cleanupFormValues(node) {  
  if (node.type && node.type === 'checkbox') {
    return { name: node.id, value: node.checked ? true : false };
  } else {
    return false;
  }
}

function showAlertMessage(message, isSuccess) {
  var alertBox = $('.alert');
  alertBox.attr('class', 'alert');
  alertBox.addClass(isSuccess ? 'alert-success' : 'alert-danger');
  alertBox.html(message);
  alertBox.fadeIn();
}