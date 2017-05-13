#!/usr/bin/env php
#
# this file autogenerates misc files from cpp to opencl
#
# requires php (apt-get install php5-cli)
# 
# on default this script runs dry, 
# it will try to generate the needed files and show which files would be modified
# this should always be run first, to see if any issues occur
# if you invoke this script with "nondry" as cli argument it will write the changes
# to the opencl files
#

<?php
require_once(dirname(__FILE__) . '/common.inc.php');

printStart();

$copyFiles['fractal_h']['path'] = PROJECT_PATH . 'src/fractal.h';
$copyFiles['fractal_h']['pathTarget'] = PROJECT_PATH . 'opencl/fractal_cl.h';
$copyFiles['fractparams_h']['path'] = PROJECT_PATH . 'src/fractparams.hpp';
$copyFiles['fractparams_h']['pathTarget'] = PROJECT_PATH . 'opencl/fractparams_cl.hpp';
$copyFiles['image_adjustments_h']['path'] = PROJECT_PATH . 'src/image_adjustments.h';
$copyFiles['image_adjustments_h']['pathTarget'] = PROJECT_PATH . 'opencl/image_adjustments_cl.h';
$copyFiles['common_params_hpp']['path'] = PROJECT_PATH . 'src/common_params.hpp';
$copyFiles['common_params_hpp']['pathTarget'] = PROJECT_PATH . 'opencl/common_params_cl.hpp';

foreach($copyFiles as $type => $copyFile){
	$oldContent = file_get_contents($copyFile['pathTarget']);
	$content = file_get_contents($copyFile['path']);

	// add the "autogen" - line to the file header
	$headerRegex = '/^(\/\*\*?[\s\S]*?\*\/)([\s\S]*)$/';
	if(!preg_match($headerRegex, $content, $matchHeader)){
		echo errorString('header unknown!');
		continue;
	}
	$fileHeader = $matchHeader[1];
	$fileSourceCode = $matchHeader[2];
	$noChangeComment = array();
	$noChangeComment[] = '      ____                   ________ ';
	$noChangeComment[] = '     / __ \____  ___  ____  / ____/ / ';
	$noChangeComment[] = '    / / / / __ \/ _ \/ __ \/ /   / /  ';
	$noChangeComment[] = '   / /_/ / /_/ /  __/ / / / /___/ /___';
	$noChangeComment[] = '   \____/ .___/\___/_/ /_/\____/_____/';
	$noChangeComment[] = '       /_/                            ';
	$noChangeComment[] = '';
	$noChangeComment[] = 'This file has been autogenerated by tools/populateOpenCL.php';
	$noChangeComment[] = 'from the file ' . str_replace(PROJECT_PATH, '', $copyFile['path']);
	$noChangeComment[] = 'D O    N O T    E D I T    T H I S    F I L E !';
	$fileHeader = str_replace('*/', '* ' . implode(PHP_EOL . ' * ', $noChangeComment) . PHP_EOL . ' */', $fileHeader);

	$content = $fileHeader . $fileSourceCode; 

	// replace opencl specific tokens (and replace all matches)
	$openCLMatchAppendCL = array(
		array('find' => '/struct\s([a-zA-Z0-9_]+)\n/'),
		array('find' => '/enum\s([a-zA-Z0-9_]+)\n/'),
		array('find' => '/const int\s([a-zA-Z0-9_]+)\s=\s/'),
	);
	foreach($openCLMatchAppendCL as $item){
		preg_match_all($item['find'], $content, $match);
		if(!empty($match[1])){
		    foreach($match[1] as $replace){
			    $content = preg_replace('/(' . $replace . ')([ \]\(;\n])/', '$1Cl$2', $content);
				$stripEnum = lcfirst(str_replace('enum', '', $replace));
				$content = preg_replace('/(' . $stripEnum . ')_([a-zA-Z0-9_]+)/', '$1Cl_$2', $content);
			}
		}
	}

	// replace opencl specific tokens
	$openCLReplaceLookup = array(
	    array('find' => '/(\s)int(\s)/', 'replace' => '$1cl_int$2'),
	    array('find' => '/(\s)bool(\s)/', 'replace' => '$1cl_int$2'),
	    array('find' => '/(\s)double(\s)/', 'replace' => '$1cl_float$2'),
	    array('find' => '/(\s)float(\s)/', 'replace' => '$1cl_float$2'),
	    array('find' => '/(\s)sRGB(\s)/', 'replace' => '$1cl_int3$2'),
	    array('find' => '/(\s)CVector3(\s)/', 'replace' => '$1cl_float3$2'),
	    array('find' => '/(\s)CVector4(\s)/', 'replace' => '$1cl_float4$2'),

        array('find' => '/struct\s([a-zA-Z0-9_]+)\n(\s*)({[\S\s]+?\n\2})/', 'replace' => "typedef struct\n$2$3 $1"),
		array('find' => '/enum\s([a-zA-Z0-9_]+)\n(\s*)({[\S\s]+?\n\2})/', 'replace' => "typedef enum\n$2$3 $1"),
		array('find' => '/const cl_int\s([a-zA-Z0-9_]+)\s=\s([a-zA-Z0-9_]+);/', 'replace' => "#define $1 $2"),
		array('find' => '/\n#include\s.*/', 'replace' => ''), // remove includes
		array('find' => '/(\s)CRotationMatrix(\s)/', 'replace' => '$1matrix33$2'),
		array('find' => '/(\s)CRotationMatrix44(\s)/', 'replace' => '$1matrix44$2'),

        array('find' => '/class\s([a-zA-Z0-9_]+);/', 'replace' => ""), // remove forward declaration
		array('find' => '/\/\/\sforward declarations/', 'replace' => ""), // remove comment "forward declaration"
		array('find' => '/MANDELBULBER2_SRC_(.*)_HPP_/', 'replace' => "MANDELBULBER2_OPENCL_$1_CL_HPP_"), // include guard 1
		array('find' => '/MANDELBULBER2_SRC_(.*)_H_/', 'replace' => "MANDELBULBER2_OPENCL_$1_CL_H_"), // include guard

        // TODO rework these regexes
		array('find' => '/namespace[\s\S]*?\n}\n/', 'replace' => ""), // no namespace support -> TODO fix files with namespaces
		array('find' => '/sParamRenderCl\([\s\S]*?\);/', 'replace' => ""), // remove constructor
		array('find' => '/sFractalCl\([\s\S]*?\);/', 'replace' => ""), // remove constructor
		array('find' => '/sImageAdjustmentsCl\([\s\S]*?}/', 'replace' => ""), // remove constructor
		array('find' => '/void RecalculateFractalParams\([\s\S]*?\);/', 'replace' => ""), // remove method

        array('find' => '/.*::.*/', 'replace' => ""), // no namespace scopes allowed?
		array('find' => '/.*cPrimitives.*/', 'replace' => ""), // need to include file...
		array('find' => '/sCommonParams /', 'replace' => "sCommonParamsCl "), // TODO autogen replace over all files
		array('find' => '/sImageAdjustments /', 'replace' => "sImageAdjustmentsCl "), // TODO autogen replace over all files

        array('find' => '/matrix44 /', 'replace' => "// matrix44 "), // TODO
	);
	foreach($openCLReplaceLookup as $item){
		$content = preg_replace($item['find'], $item['replace'], $content);
	}

    // add c++ side includes
	$cppIncludes = '#ifndef OPENCL_KERNEL_CODE' . PHP_EOL;
	$cppIncludes .= '#include "../src/fractal_enums.h"' . PHP_EOL;
	$cppIncludes .= '#include "../opencl/opencl_algebra.h"' . PHP_EOL;
	$cppIncludes .= '#include "../opencl/common_params_cl.hpp"' . PHP_EOL;
	$cppIncludes .= '#include "../opencl/image_adjustments_cl.h"' . PHP_EOL;
	$cppIncludes .= '#include "../src/common_params.hpp"' . PHP_EOL;
	$cppIncludes .= '#include "../src/image_adjustments.h"' . PHP_EOL;
	$cppIncludes .= '#include "../src/fractparams.hpp"' . PHP_EOL;
	$cppIncludes .= '#include "../src/fractal.h"' . PHP_EOL;

    $cppIncludes .= '#endif' . PHP_EOL;
	$content = preg_replace('/(#define MANDELBULBER2_OPENCL_.*)/', '$1' . PHP_EOL . PHP_EOL . $cppIncludes, $content);

    // create copy methods for structs
	preg_match_all('/typedef struct\n{([\s\S]*?)}\s([0-9a-zA-Z_]+);/', $content, $structMatches);
	$copyStructs = array();
	foreach($structMatches[1] as $key => $match){
	    $props = array();
		$structName = trim($structMatches[2][$key]);
		$lines = explode(PHP_EOL, $match);
		foreach($lines as $line){
		    $line = trim($line);
			if(preg_match('/^\s*([a-zA-Z0-9_]+)\s([a-zA-Z0-9_]+);.*/', $line, $lineMatch)){
			    $prop = array();
				$prop['name'] = $lineMatch[2];
				$prop['typeName'] = $lineMatch[1];
				$prop['type'] = $lineMatch[1];
				if(substr($prop['type'], 0, 1) == 's') $prop['type'] = 'struct';
				if(substr($prop['type'], 0, 4) == 'enum') $prop['type'] = 'enum';
				$props[] = $prop;
			}
		}
		$copyStructs[] = getCopyStruct($structName, $props);
	}
	$content = preg_replace('/(#endif \/\* MANDELBULBER2_OPENCL.*)/',
	    PHP_EOL . '#ifndef OPENCL_KERNEL_CODE' . PHP_EOL
		. implode(PHP_EOL, $copyStructs) . PHP_EOL . '#endif' . PHP_EOL . PHP_EOL . '$1', $content);

    // clang-format
	$filepathTemp = $copyFile['path'] . '.tmp.c';
	file_put_contents($filepathTemp, $content);
	shell_exec('clang-format -i --style=file ' . escapeshellarg($filepathTemp));
	$content = file_get_contents($filepathTemp);
	unlink($filepathTemp); // nothing to see here :)
	
	if($content != $oldContent){
		if(!isDryRun()){
			file_put_contents($copyFile['pathTarget'], $content);
		}
		echo successString('file ' . $copyFile['pathTarget'] . ' changed.') . PHP_EOL;
	}else{
		if(isVerbose()){
			echo noticeString('file ' . $copyFile['pathTarget'] . ' has not changed.') . PHP_EOL;
		}
	}
}

function getCopyStruct($structName, $properties){
    $structNameSource = substr($structName, 0, -2);
	$out = 'inline ' . $structName . ' clCopy' . ucfirst($structName) . '(' . $structNameSource . ' source){' . PHP_EOL;
	$out .= '	' . $structName . ' target;' . PHP_EOL;
    foreach($properties as $property){
        $copyLine = 'target.' . $property['name'] . ' = ';
		switch($property['type']){
		case 'struct': $copyLine .= 'clCopy' . ucfirst($property['typeName']) . '(source.' . $property['name'] . ');'; break;
		    case 'enum': $copyLine .=  $property['typeName'] . '(source.' . $property['name'] . ');'; break;
			case 'cl_float3': $copyLine .= 'toClFloat3(source.' . $property['name'] . ');'; break;
			case 'matrix33': $copyLine .= 'toClMatrix33(source.' . $property['name'] . ');'; break;
			case 'cl_int3': $copyLine .= 'toClInt3(source.' . $property['name'] . ');'; break;
			case 'cl_float4': $copyLine .= 'toClFloat4(source.' . $property['name'] . ');'; break;

            default:  $copyLine .= 'source.' . $property['name'] . ';';
        }
		$out .= '	' . $copyLine . PHP_EOL;
    }

    $out .= '	' . 'return target;' . PHP_EOL;
	$out .= '}' . PHP_EOL;
    return $out;
}

printFinish();
exit;

?>

