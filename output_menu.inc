<?php

$pages = Array(
	'name' => "FPP Arcade",
	'type' => "output",
	'page' => "joysticks.php");

?>

<li><a href="<?php echo preg_replace('/.*\/plugins\/(.*)/', 'plugin.php?plugin=$1&page=joysticks.php', dirname(__FILE__)); ?>">FPP Arcade Joysticks</a></li>
