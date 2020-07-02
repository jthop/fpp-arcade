<?php
$data = file_get_contents('http://127.0.0.1:32322/arcade/controllers');
$controllers = json_decode($data, true);
?>
<script language="Javascript">
allowMultisyncCommands = true;

function RefreshLastMessages() {
    $.get('api/plugin-apis/arcade/events', function (data) {
          $("#lastMessages").text(data);
        }
    );
}
    
function GetJS(button) {
    var js = {};
    js["enabled"] = $("#" + button + "_enabled").is(':checked');
    js["pressed"] = {};
    js["released"] = {};
    CommandToJSON(button + "_PressedCommand", "tablePressed" + button, js["pressed"]);
    CommandToJSON(button + "_ReleasedCommand", "tableReleased" + button, js["released"]);

    return js;
}
function SaveJoystickInputs() {
    var js = Array();
    
<?
    $count = 0;
    foreach($controllers as $controller) {
        $contName = $controller['name'];
        $contNameClean = str_replace("-", "_", $contName);
        $contNameClean = str_replace(":", "_", $contName);
        $contNameClean = str_replace(" ", "_", $contNameClean);
        for ($x = 0; $x < $controller['buttons']; $x++) {
            $buttonNameClean = $contNameClean . "_" . $x;
            echo "    var jsb = GetJS('" . $buttonNameClean . "');\n";
            echo "    jsb['controller'] = '" . $contName . "';\n";
            echo "    jsb['button'] = " . $x . ";\n";
            echo "    js.push(jsb);\n";
        }
    }
?>
    var postData = JSON.stringify(js, null, 4);
    $.ajax({
            url: "api/configfile/joysticks.json",
            type: 'POST',
            contentType: 'application/json',
            data: postData,
            dataType: 'json',
            success: function(data) {
                    $('html,body').css('cursor','auto');
                    if (data.Status == 'OK') {
                        $.jGrowl("Joystick Inputs Saved.");
                        SetRestartFlag(2);
                    } else {
                        alert('ERROR: ' + data.Message);
                    }
            },
            error: function() {
                    $('html,body').css('cursor','auto');
                    alert('Error, Failed to save Joystick inputs');
            }
    });
    
}

/////////////////////////////////////////////////////////////////////////////
$(document).ready(function(){
});
</script>

<!-- --------------------------------------------------------------------- -->



  <div style="overflow: hidden; padding: 10px;">
    <span style="float:right">
    <table border=0>
    <tr><td style='vertical-align: top;'>Last Events:&nbsp;<input type="button" value="Refresh" class="buttons" onclick="RefreshLastMessages();"></td></tr><tr><td style='vertical-align: top;'><pre id="lastMessages" style='min-width:200px; margin:1px;'></pre></td></tr>
    </table>
    </span>
    <div>
        <input type="button" value="Save" class="buttons" onClick="SaveJoystickInputs();"></input>
    </div>
    <div class='fppTableWrapper'>
    <div class='fppTableContents fullWidth'>
    <table id='JoystickInputs' width="100%">
        <thead>
            <tr>
                <th rowspan='2'>En.</th>
                <th rowspan='2'>Controller</th>
                <th rowspan='2'>Button #</th>
                <th colspan='2'>Commands</th>
            </tr>
            <tr>
                <th >Pressed</th>
                <th >Released</th>
            </tr>
        </thead>
        <tbody>
<?
$count = 0;
foreach($controllers as $controller) {
    $contName = $controller['name'];
    $contNameClean = str_replace("-", "_", $contName);
    $contNameClean = str_replace(":", "_", $contName);
    $contNameClean = str_replace(" ", "_", $contNameClean);
    for ($x = 0; $x < $controller['buttons']; $x++) {
        $buttonNameClean = $contNameClean . "_" . $x;
        $style = " evenRow";
        if ($count % 2 == 0) {
            $style = " oddRow";
        }
        $count = $count + 1;
        ?>
            <tr class='fppTableRow <?= $style ?>' id='row_<?= $count ?>'>
                <td><input type="checkbox" id="<?= $buttonNameClean ?>_enabled"></td>
                <td><?= $contName ?></td>
                <td><?= $x ?></td>
                <td>
                    <table border=0 class='fppTable' id='tablePressed<?= $buttonNameClean ?>'>
                    <tr>
        <td>Command:</td><td><select id='<?= $buttonNameClean ?>_PressedCommand' onChange='CommandSelectChanged("<?= $buttonNameClean ?>_PressedCommand", "tablePressed<?= $buttonNameClean ?>", false);'><option value=""></option></select></td>
                    </tr>
                    </table>
                </td>
                <td>
                        <table border=0 class='fppTable' id='tableReleased<?= $buttonNameClean ?>'>
                        <tr>
        <td>Command:</td><td><select id='<?= $buttonNameClean ?>_ReleasedCommand' onChange='CommandSelectChanged("<?= $buttonNameClean ?>_ReleasedCommand", "tableReleased<?= $buttonNameClean ?>", false);'><option value=""></option></select></td>
                        </tr>
                        </table>
                </td>
                <script>
                LoadCommandList($('#<?= $buttonNameClean ?>_PressedCommand'));
                LoadCommandList($('#<?= $buttonNameClean ?>_ReleasedCommand'));
                </script>
        </tr>
<?
    }
}
?>
</tbody>
</table>
</div>
</div>
<script>
<?

if (file_exists('/home/fpp/media/config/joysticks.json')) {
    $data = file_get_contents('/home/fpp/media/config/joysticks.json');
    $jsInputJson = json_decode($data, true);
    echo "var joystickConfig = " . json_encode($jsInputJson, JSON_PRETTY_PRINT) . ";\n";

    $x = 0;
    foreach($jsInputJson as $js) {
        $contName = $js['controller'];
        $contNameClean = str_replace("-", "_", $contName);
        $contNameClean = str_replace(":", "_", $contName);
        $contNameClean = str_replace(" ", "_", $contNameClean);

        $button = $js['button'];
        $buttonNameClean = $contNameClean . "_" . $button;

        if ($js['enabled'] == true) {
            echo "$('#" . $buttonNameClean . "_enabled').prop('checked', true);\n";
        }


        echo "PopulateExistingCommand(joystickConfig[" . $x . "][\"pressed\"], '" . $buttonNameClean . "_PressedCommand', 'tablePressed" . $buttonNameClean . "', false);\n";
        echo "PopulateExistingCommand(joystickConfig[" . $x . "][\"released\"], '" . $buttonNameClean . "_ReleasedCommand', 'tableReleased" . $buttonNameClean . "', false);\n";
        $x = $x + 1;
    }
}
?>
RefreshLastMessages();
</script>

	</div>
