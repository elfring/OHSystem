<?php
if (!isset($website) ) { header('HTTP/1.1 404 Not Found'); die; }
?>
<div align="center">
<?php
 $upd = 1;
 
 if ( isset($_GET["update"]) ) {
    CreateHeroList("../inc/cache/");
 }
		  
if ( isset($_GET["del"]) ) {
   $res = 1;
   $sth = $db->prepare("DELETE FROM ".OSDB_HEROES." WHERE heroid = '".safeEscape($_GET["del"])."' LIMIT 1 ");
   $result = $sth->execute();
   if ($res) {
   ?>
   <h2>Hero successfully deleted</h2>
   <?php
   CreateHeroList("../inc/cache/");
   OS_AddLog($_SESSION["username"], "[os_heroes] DELETED HERO ( ".safeEscape($_GET["del"])." )"); 
   }
}

    if ( isset($_GET["hid"]) AND isset($_GET["type"]) ) {
     $hid = safeEscape($_GET["hid"]);
	 $type = (int) $_GET["type"];
	 $update = $db->update(OSDB_HEROES, array(
		   "type" => $type
		   ), 
	"heroid = '".$hid."' ");
	CreateHeroList("../inc/cache/");
    }

if ( isset($_GET["edit"]) OR isset($_GET["add"]) ) {

if ( isset($_GET["edit"]))  $edit = safeEscape( $_GET["edit"]);

   if ( isset($_GET["add"]) ) {
     $HeroName = "";
	 $heroid = "";
	 $desc = "";
	 $stats = "";
	 $skills = "";
	 $type = 0;
   }
   
   if ( isset($_POST["edit_hero"])  ) {
      $HeroName = EscapeStr($_POST["hero_name"]);
	  $heroid = EscapeStr($_POST["heroid"]);
	  $desc = (my_nl2br((trim($_POST["desc"]))));
	  $desc = str_replace(array("&Scaron;", "&scaron;"),array("Š","š"), $desc   );
	  
	  $type = (int) $_POST["type"];
	  
	  $stats = (my_nl2br(removeDoubleSpaces(trim($_POST["stats"]))));
	  $stats = str_replace(array("&Scaron;", "&scaron;"),array("Š","š"), $stats   );
	  
	  $skills = (my_nl2br(removeDoubleSpaces(trim($_POST["skills"]))));
	  $skills = str_replace(array("&Scaron;", "&scaron;"),array("Š","š"), $skills   );
	  
	  if ( $heroid!="" AND strlen($HeroName)>=2 ) {
	  
	  if ( isset($_GET["edit"]) ) {
         $update = $db->update(OSDB_HEROES, array(
		   "description" => $HeroName, 
		   "summary" => $desc, 
		   "stats" => $stats, 
		   "skills" => $skills,
		   "type" => $type
		   ), 
		                        "heroid = '".$edit."' ");
		OS_AddLog($_SESSION["username"], "[os_heroes] EDITED HERO ( $edit, $HeroName )"); 
		
		} else {
		$hid = str_replace(".gif", "", $heroid);
		$check = $db->prepare("SELECT * FROM ".OSDB_HEROES." WHERE heroid = '".$hid."' ");
		$result = $check->execute();
		if ($check->rowCount()>=1 ) {
		?><h2>Hero already exists</h2><?php
		} else {
		  //INSERT

		  $db->insert( OSDB_HEROES, array(
		  "heroid" => $hid,
		  "original" => $hid,
		  "description" => $HeroName,
		  "summary" => $desc,
		  "stats" => $stats,
		  "skills" => $skills,
		  "type" => $type
                                 ));
		 OS_AddLog($_SESSION["username"], "[os_heroes] ADD HERO ( $hid, $HeroName )"); 
		}
		}
		
		if ($upd) {
		?><h2>Hero successfully updated</h2><?php
		}
	  } else echo "<h2>Missing HeroID or Hero Name does not have enought characters </h2>";
	  CreateHeroList("../inc/cache/");
   }

   if ( isset($_GET["edit"]) AND !isset($_GET["add"]) ) {
   $sth = $db->prepare("SELECT * FROM ".OSDB_HEROES." WHERE heroid = '".$edit."' LIMIT 1");
   $result = $sth->execute();
   $row = $sth->fetch(PDO::FETCH_ASSOC);
   $description = $row["description"];
   $original = $row["original"];
   $summary = $row["summary"];
   $stats = $row["stats"];
   $skills = $row["skills"];
   $type = $row["type"];
   $button = "Edit Hero";
   } else {
   $description ="";
   $original = "A16S";
   $summary = "";
   $stats = "";
   $skills = "";
   $button = "ADD Hero";
   $type = 0;
   }
   
   ?>
 <h2>Edit Hero</h2>
 <form action method="post">
 <table class="Table500px">
   <tr>
   <td>
   <input style="width: 400px; height: 34px; background-color: #fafafa; color: #000;" type="text" value="<?=$description?>" name="hero_name" size="75" maxlength="254" />
   </td>
   </tr>
   <tr>
   <td>
   <div class="padTop"></div>
   <img id="himg" style="vertical-align: top;" src="<?=$website?>img/heroes/<?=$original?>.gif" alt="" />
   <?php
   if ($handle = opendir("../img/heroes")) {
   ?>
   <select onchange = "getHimg()" name="heroid" id="heroid">
   <?php
   while (false !== ($file = readdir($handle))) 
	{
	  if (substr($file,-3) == "gif" ) {
	  
	  if (trim( str_replace(".php", "", $file) ) == trim($original).".gif")  {
	  $sel="selected";
	  $cls = 'style="background-color: yellow"';
	  }
	  else { $sel = ""; $cls = ""; }
	  ?>
	  <option <?=$cls ?> <?=$sel?> value="<?=$file?>"><?=$file?></option>
	  <?php
	 }
	}
   ?>
   </select>
   <?php } ?>
 <div class="padTop"></div>
 
 <select name="type">
 <?php if ($type == 0) $sel = 'selected="selected"'; else $sel = ""; ?>
    <option <?=$sel?> value="0">Strength</option>
 <?php if ($type == 1) $sel = 'selected="selected"'; else $sel = ""; ?>
	<option <?=$sel?> value="1">Agility</option>
 <?php if ($type == 2) $sel = 'selected="selected"'; else $sel = ""; ?>
	<option <?=$sel?> value="2">Intelligence</option>
 </select>
 
   </td>
   </tr>

   <tr>
   <th class="padLeft">Description</th>
   </tr>
   
   <tr>
     <td>
       <textarea class="ckeditor" cols="90" id="editor1" name="desc" rows="20"><?=$summary?></textarea>
     </td>
   </tr>
   <tr>
   <th class="padLeft">Stats</th>
   </tr>
   
   <tr>
     <td>
       <textarea class="ckeditor" cols="90" id="editor2" name="stats" rows="20"><?=$stats?></textarea>
     </td>
   </tr>
   
   <tr>
   <th class="padLeft">Skills</th>
   </tr>
   
   <tr>
     <td>
       <textarea class="ckeditor" cols="90" id="editor3" name="skills" rows="20"><?=$skills?></textarea>
     </td>
   </tr>
   
   <tr>
     <td>
       <div class="padTop"></div>
	   <input type="submit" value="<?= $button?>" class="menuButtons" name="edit_hero" />
	   <span class="padLeft">
<?php if (isset($_GET["edit"]) ) { ?>
	   <input type="button" onclick="if (confirm('Delete Hero?') ) { location.href='<?=$website?>adm/?heroes&amp;del=<?=$edit?>' }" value="Delete Hero" class="menuButtons" /><?php } ?>
	   </span>
	   <div class="padTop"></div>
     </td>
   </tr>
   
 </table>
 </form>
 <script type="text/javascript" src="<?php echo $website;?>adm/editor.js"></script>
 
 <script type="text/javascript" >
  function getHimg() {
    var img = document.getElementById("heroid").value;
	document.getElementById("himg").src = "../img/heroes/"+img+"?"+Math.random()*100;
  }
 </script>
   <?php
}
  
if ( isset($_GET["show_all"])  ) {
$sql="";
$qry = "SELECT COUNT(*) FROM ".OSDB_HEROES." WHERE summary!= '-'";
$sth = $db->prepare($qry);
$result = $sth->execute();
  $r = $sth->fetch(PDO::FETCH_NUM);
  $numrows = $r[0];
  $result_per_page = 30;
  $draw_pagination = 1;
}
else {
$sql = "GROUP BY (description)";
$qry = "SELECT * FROM ".OSDB_HEROES." WHERE summary!= '-' $sql";
$sth = $db->prepare($qry);
$result = $sth->execute();
  $row = $sth->fetch(PDO::FETCH_ASSOC);
  $numrows = $sth->rowCount();
  $result_per_page = 30;
  $draw_pagination = 1;
}
  
  $SHOW_TOTALS = 1;
  include('pagination.php');
  
    $sth  = $db->prepare("SELECT * FROM ".OSDB_HEROES." WHERE original!='' AND summary!= '-' 
	$sql 
	ORDER BY (description) ASC 
    LIMIT $offset, $rowsperpage");
	$result = $sth->execute();
?>
<a href="<?=$website?>adm/?heroes&amp;add" class="menuButtons">[+] ADD Hero</a> 
<a href="<?=$website?>adm/?heroes&amp;update" class="menuButtons">Update Heroes Data</a>
<?php if (!isset($_GET["show_all"]) ) { ?>
<a href="<?=$website?>adm/?heroes&amp;show_all" class="menuButtons">Display ALL</a>
<?php } else { ?>
<a href="<?=$website?>adm/?heroes" class="menuButtons">Display Default</a>
<?php } ?>
  <table>
    <tr>
	  <th width="80" class="padLeft">Hero</th>
	  <th width="220">Hero name</th>
	  <th width="380">Summary</th>
	  <th>Guides</th>
	</tr>
<?php
  while ($row = $sth->fetch(PDO::FETCH_ASSOC)) { 
  $type = $row["type"];
  
  if ( isset($_GET["hid"]) AND $_GET["hid"] == $row["heroid"] ) $style = 'style="border:4px solid #920000"'; else $style = "";
  ?>
  <tr>
    <td class="padLeft" <?=$style?>><a href="<?=$website?>adm/?heroes&amp;edit=<?=$row["heroid"]?>"><img src="<?=$website?>img/heroes/<?=$row["original"]?>.gif" alt="" /></a></td>
	<td width="220" <?=$style?>>
	<a href="<?=$website?>adm/?heroes&amp;edit=<?=$row["heroid"]?>"><?=$row["description"]?></a>
	<div>
	<form action="" method="get">
	 <input type="hidden" name="heroes" />
	 <input type="hidden" name="hid" value="<?=$row["heroid"]?>" />
 <select name="type">
 <?php if ($type == 0) $sel = 'selected="selected"'; else $sel = ""; ?>
    <option <?=$sel?> value="0">Strength</option>
 <?php if ($type == 1) $sel = 'selected="selected"'; else $sel = ""; ?>
	<option <?=$sel?> value="1">Agility</option>
 <?php if ($type == 2) $sel = 'selected="selected"'; else $sel = ""; ?>
	<option <?=$sel?> value="2">Intelligence</option>
 </select>
    <?php if (isset($_GET["page"]) AND is_numeric($_GET["page"]) ) { ?>
	 <input type="hidden" name="page" value="<?=(int)$_GET["page"]?>" />
	<?php } ?>
	<input type="submit" value="save" class="menuButtons" />
	</form>
	</div>
	</td>
	<td><?=limit_words($row["summary"], 20)?></td>
	<td><a href="<?=$website?>adm/?guides&amp;hid=<?=$row["original"]?>" class="menuButtons">Add a guide</a></td>
   </tr>
  <?php
  }
   ?>
   </table>
<?php 
include('pagination.php');

?>
</div>