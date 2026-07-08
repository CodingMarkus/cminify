#!/usr/bin/env sh

assert()
{
	result="$(printf '%b' "$2" | ./build/cminify js - --mangle-js-identifiers)"
	if [ "$?" != "0" ]; then
		echo Crashed on:
		echo "$2"
		echo Standard output:
		echo "$result"
		exit 1
	elif [ "$1" != "$result" ]; then
		echo 'Error: expected:'
		echo "$1"
		echo got:
		echo "$result"
		exit 1
	fi
}

assert_without_mangling()
{
	result="$(printf '%b' "$2" | ./build/cminify js -)"
	if [ "$?" != "0" ]; then
		echo Crashed on:
		echo "$2"
		echo Standard output:
		echo "$result"
		exit 1
	elif [ "$1" != "$result" ]; then
		echo 'Error: expected:'
		echo "$1"
		echo got:
		echo "$result"
		exit 1
	fi
}

input='function demo(longName){return longName}'
expected='function demo(longName){return longName}'
assert_without_mangling "$expected" "$input"

input='function demo(longName){return longName}'
expected='function demo(a){return a}'
assert "$expected" "$input"

input='function demo(longName){let otherName=longName+1;return otherName}'
expected='function demo(a){let b=a+1;return b}'
assert "$expected" "$input"

input='function demo(firstName,secondName){var totalName=firstName+'\
'secondName;return totalName}'
expected='function demo(a,b){var c=a+b;return c}'
assert "$expected" "$input"

input='function demo(firstName){let secondName=1,thirdName=2;'\
'return firstName+secondName+thirdName}'
expected='function demo(a){let b=1,c=2;return a+b+c}'
assert "$expected" "$input"

input='function demo(firstName){const secondName=1;return firstName+'\
'secondName}'
expected='function demo(a){const b=1;return a+b}'
assert "$expected" "$input"

input='function demo(firstName=defaultValue){return firstName+defaultValue}'
expected='function demo(a=defaultValue){return a+defaultValue}'
assert "$expected" "$input"

input='function demo(...restName){return restName.length}'
expected='function demo(...a){return a.length}'
assert "$expected" "$input"

input='function demo([firstName,secondName]){return firstName+secondName}'
expected='function demo([a,b]){return a+b}'
assert "$expected" "$input"

input='function demo({sourceName:localName}){return localName}'
expected='function demo({sourceName:a}){return a}'
assert "$expected" "$input"

input='function demo({localName}){return localName}'
expected='function demo({localName:a}){return a}'
assert "$expected" "$input"

input='let demo=longName=>longName+1'
expected='let demo=a=>a+1'
assert "$expected" "$input"

input='let demo=(firstName=defaultValue)=>firstName+defaultValue'
expected='let demo=(a=defaultValue)=>a+defaultValue'
assert "$expected" "$input"

input='let demo=(...restName)=>restName.length'
expected='let demo=(...a)=>a.length'
assert "$expected" "$input"

input='let demo=(firstName,secondName)=>firstName+secondName'
expected='let demo=(a,b)=>a+b'
assert "$expected" "$input"

input='let demo=([firstName,secondName])=>firstName+secondName'
expected='let demo=([a,b])=>a+b'
assert "$expected" "$input"

input='let demo=({sourceName:localName})=>localName'
expected='let demo=({sourceName:a})=>a'
assert "$expected" "$input"

input='let demo=({localName})=>localName'
expected='let demo=({localName:a})=>a'
assert "$expected" "$input"

input='let demo=longName=>{let otherName=longName+1;return otherName}'
expected='let demo=a=>{let b=a+1;return b}'
assert "$expected" "$input"

input='let topLevel=1;function demo(longName){return topLevel+longName}'
expected='let topLevel=1;function demo(a){return topLevel+a}'
assert "$expected" "$input"

input='let moduleGlobal=1;function demo(longName)'\
'{return moduleGlobal+longName}'
expected='let moduleGlobal=1;function demo(a){return moduleGlobal+a}'
assert "$expected" "$input"

input='var classicGlobal=1;let secondClassic=2;const thirdClassic=3;'\
'function demo(){return classicGlobal+secondClassic+thirdClassic}'
expected='var classicGlobal=1;let secondClassic=2;const thirdClassic=3;'\
'function demo(){return classicGlobal+secondClassic+thirdClassic}'
assert "$expected" "$input"

input='"import";let classicGlobal=1;function demo(){return classicGlobal}'
expected='"import";let classicGlobal=1;function demo(){return classicGlobal}'
assert "$expected" "$input"

input='/*! export */let classicGlobal=1;function demo(){return classicGlobal}'
expected='/*! export */let classicGlobal=1;'\
'function demo(){return classicGlobal}'
assert "$expected" "$input"

input='import "./mod";let moduleGlobal=1;function demo(longName)'\
'{return moduleGlobal+longName}'
expected='import"./mod";let g0=1;function demo(a){return g0+a}'
assert "$expected" "$input"

input='import defaultName from "./mod";function demo(){return defaultName}'
expected='import g0 from"./mod";function demo(){return g0}'
assert "$expected" "$input"

input='import * as namespaceName from "./mod";function demo()'\
'{return namespaceName.value}'
expected='import*as g0 from"./mod";function demo(){return g0.value}'
assert "$expected" "$input"

input='import {sourceName as localName} from "./mod";function demo()'\
'{return localName}'
expected='import{sourceName as g0}from"./mod";function demo(){return g0}'
assert "$expected" "$input"

input='import {bareName} from "./mod";function demo(){return bareName}'
expected='import{bareName as g0}from"./mod";function demo(){return g0}'
assert "$expected" "$input"

input='import defaultName,{sourceName as localName,bareName} from "./mod";'\
'function demo(){return defaultName+localName+bareName}'
expected='import g0,{sourceName as g1,bareName as g2}from"./mod";'\
'function demo(){return g0+g1+g2}'
assert "$expected" "$input"

input='import "./mod";let g0=1;let moduleGlobal=2;'\
'function demo(){return g0+moduleGlobal}'
expected='import"./mod";let g0=1;let g1=2;function demo(){return g0+g1}'
assert "$expected" "$input"

input='import "./mod";const moduleGlobal=1;var secondGlobal=2;'\
'function demo(){return moduleGlobal+secondGlobal}'
expected='import"./mod";const g0=1;var g1=2;function demo(){return g0+g1}'
assert "$expected" "$input"

input='import "./mod";function moduleFunction(){return 1}moduleFunction()'
expected='import"./mod";function g0(){return 1}g0()'
assert "$expected" "$input"

input='import "./mod";export function moduleFunction(){return 1}'\
'moduleFunction()'
expected='import"./mod";export function moduleFunction(){return 1}'\
'moduleFunction()'
assert "$expected" "$input"

input='import "./mod";export const exportedGlobal=1;const localGlobal=2;'\
'function demo(){return exportedGlobal+localGlobal}'
expected='import"./mod";export const exportedGlobal=1;const g0=2;'\
'function demo(){return exportedGlobal+g0}'
assert "$expected" "$input"

input='import "./mod";export {localGlobal};const localGlobal=2;'\
'function demo(){return localGlobal}'
expected='import"./mod";export{localGlobal};const localGlobal=2;'\
'function demo(){return localGlobal}'
assert "$expected" "$input"

input='import "./mod";export {localGlobal as publicGlobal};'\
'const localGlobal=2;function demo(){return localGlobal}'
expected='import"./mod";export{localGlobal as publicGlobal};'\
'const localGlobal=2;function demo(){return localGlobal}'
assert "$expected" "$input"

input='import "./mod";export default localGlobal;const localGlobal=2;'\
'function demo(){return localGlobal}'
expected='import"./mod";export default localGlobal;const localGlobal=2;'\
'function demo(){return localGlobal}'
assert "$expected" "$input"

input='import "./mod";export default function moduleFunction()'\
'{return moduleFunction()}'
expected='import"./mod";export default function moduleFunction()'\
'{return moduleFunction()}'
assert "$expected" "$input"

input='import "./mod";const publicName=1;export {publicName};'\
'const localGlobal=2;function demo(){return publicName+localGlobal}'
expected='import"./mod";const publicName=1;export{publicName};'\
'const g0=2;function demo(){return publicName+g0}'
assert "$expected" "$input"

input='import "./mod";const localName=1;export {localName as publicName};'\
'const otherName=2;function demo(){return localName+otherName}'
expected='import"./mod";const localName=1;export{localName as publicName};'\
'const g0=2;function demo(){return localName+g0}'
assert "$expected" "$input"

input='import "./mod";export {sourceName as publicName} from "./other";'\
'const localGlobal=1;function demo(){return localGlobal}'
expected='import"./mod";export{sourceName as publicName}from"./other";'\
'const g0=1;function demo(){return g0}'
assert "$expected" "$input"

input='import {sourceName as localName} from "./mod";export {localName};'\
'function demo(){return localName}'
expected='import{sourceName as localName}from"./mod";export{localName};'\
'function demo(){return localName}'
assert "$expected" "$input"

input='import "./mod";let {moduleGlobal}=source;'\
'function demo(){return moduleGlobal}'
expected='import"./mod";let{moduleGlobal:g0}=source;'\
'function demo(){return g0}'
assert "$expected" "$input"

input='import "./mod";let moduleGlobal=1;'\
'function demo(){return {moduleGlobal}}'
expected='import"./mod";let g0=1;function demo(){return{g0:g0}}'
assert "$expected" "$input"

input='import "./mod";let moduleGlobal=1;'\
'function demo(){return /moduleGlobal/.test(moduleGlobal)}'
expected='import"./mod";let g0=1;'\
'function demo(){return/moduleGlobal/.test(g0)}'
assert "$expected" "$input"

input='import "./mod";let moduleGlobal=1;eval("moduleGlobal");'\
'function demo(longName){return moduleGlobal+longName}'
expected='import"./mod";let moduleGlobal=1;eval("moduleGlobal");'\
'function demo(longName){return moduleGlobal+longName}'
assert "$expected" "$input"

input='with(obj);function demo(longName){return longName}'
expected='with(obj);function demo(longName){return longName}'
assert "$expected" "$input"

input='import "./mod";let moduleGlobal=1;function demo(moduleGlobal)'\
'{return moduleGlobal}'
expected='import"./mod";let g0=1;function demo(a){return a}'
assert "$expected" "$input"

input='import "./mod";let moduleGlobal=1;function demo(){let moduleGlobal=2;'\
'return moduleGlobal}'
expected='import"./mod";let g0=1;function demo(){let a=2;return a}'
assert "$expected" "$input"

input='function demo(longName){return longName.value+longName["value"]}'
expected='function demo(a){return a.value+a["value"]}'
assert "$expected" "$input"

input='function demo(longName){return {fixed:longName}}'
expected='function demo(a){return{fixed:a}}'
assert "$expected" "$input"

input='function demo(longName){return {longName:longName}}'
expected='function demo(a){return{longName:a}}'
assert "$expected" "$input"

input='function demo(longName){return /longName/.test(longName)}'
expected='function demo(a){return/longName/.test(a)}'
assert "$expected" "$input"

input='function demo(longName){/*! longName */return "longName"+longName}'
expected='function demo(a){/*! longName */return"longName"+a}'
assert "$expected" "$input"

input='function demo(longName){let obj={longName};return obj}'
expected='function demo(a){let b={a:a};return b}'
assert "$expected" "$input"

input='function demo(){try{}catch(longName){return longName}}'
expected='function demo(){try{}catch(a){return a}}'
assert "$expected" "$input"

input='function demo(){let [firstName,secondName]=pair;'\
'return firstName+secondName}'
expected='function demo(){let[a,b]=pair;return a+b}'
assert "$expected" "$input"

input='function demo(){let {sourceName:localName}=obj;return localName}'
expected='function demo(){let{sourceName:a}=obj;return a}'
assert "$expected" "$input"

input='function demo(){let {localName}=obj;return localName}'
expected='function demo(){let{localName:a}=obj;return a}'
assert "$expected" "$input"

input='function demo(){let [firstName=defaultValue,...restName]=pair;'\
'return firstName+restName.length+defaultValue}'
expected='function demo(){let[a=defaultValue,...b]=pair;'\
'return a+b.length+defaultValue}'
assert "$expected" "$input"

input='function demo(){let {sourceName:localName=defaultValue}=obj;'\
'return localName+defaultValue}'
expected='function demo(){let{sourceName:a=defaultValue}=obj;'\
'return a+defaultValue}'
assert "$expected" "$input"

input='function demo(){let {localName=defaultValue}=obj;'\
'return localName+defaultValue}'
expected='function demo(){let{localName:a=defaultValue}=obj;'\
'return a+defaultValue}'
assert "$expected" "$input"

input='function demo(){let {outerName:{innerName}}=obj;return innerName}'
expected='function demo(){let{outerName:{innerName:a}}=obj;return a}'
assert "$expected" "$input"

input='function demo(){let {outerName:{innerName=defaultValue}}=obj;'\
'return innerName+defaultValue}'
expected='function demo(){let{outerName:{innerName:a=defaultValue}}=obj;'\
'return a+defaultValue}'
assert "$expected" "$input"

input='function demo(){let {...restName}=obj;return restName.value}'
expected='function demo(){let{...a}=obj;return a.value}'
assert "$expected" "$input"

input='function demo({outerName:{innerName}}){return innerName}'
expected='function demo({outerName:{innerName:a}}){return a}'
assert "$expected" "$input"

input='function demo({localName,...restName})'\
'{return localName+restName.value}'
expected='function demo({localName:a,...b}){return a+b.value}'
assert "$expected" "$input"

input='function demo(longName){loopLabel:while(longName)break loopLabel}'
expected='function demo(a){loopLabel:while(a)break loopLabel}'
assert "$expected" "$input"

input='function demo(loopLabel){loopLabel:while(loopLabel)continue loopLabel}'
expected='function demo(a){loopLabel:while(a)continue loopLabel}'
assert "$expected" "$input"

input='function demo(){let loopLabel=1;loopLabel:while(loopLabel)'\
'break loopLabel;return loopLabel}'
expected='function demo(){let a=1;loopLabel:while(a)break loopLabel;'\
'return a}'
assert "$expected" "$input"

input='function demo(longName){eval(longName);return longName}'
expected='function demo(longName){eval(longName);return longName}'
assert "$expected" "$input"

input='function demo(longName){obj.eval(longName);return longName}'
expected='function demo(a){obj.eval(a);return a}'
assert "$expected" "$input"

input='import "./mod";let moduleGlobal=1;obj.eval("moduleGlobal");'\
'function demo(){return moduleGlobal}'
expected='import"./mod";let g0=1;obj.eval("moduleGlobal");'\
'function demo(){return g0}'
assert "$expected" "$input"

input='function demo(longName){with(longName)return longName}'
expected='function demo(longName){with(longName)return longName}'
assert "$expected" "$input"

input='function demo(longName){eval("");function innerName(otherName)'\
'{return otherName}return longName}'
expected='function demo(longName){eval("");function innerName(otherName)'\
'{return otherName}return longName}'
assert "$expected" "$input"

input='function demo(longName){function innerName(otherName)'\
'{eval(otherName);return otherName}return longName}'
expected='function demo(a){function innerName(otherName)'\
'{eval(otherName);return otherName}return a}'
assert "$expected" "$input"

input='function demo(longName){function innerName(){return longName}'\
'return innerName()}'
expected='function demo(a){function b(){return a}return b()}'
assert "$expected" "$input"

input='function demo(longName){function innerName(otherName)'\
'{let localName=otherName;return localName}return innerName(longName)}'
expected='function demo(a){function b(a){let b=a;return b}return b(a)}'
assert "$expected" "$input"

input='function demo(longName){function innerName(otherName)'\
'{return innerName(longName)+otherName}return innerName(1)}'
expected='function demo(a){function b(c){return b(a)+c}return b(1)}'
assert "$expected" "$input"

input='function demo(longName){if(longName){function innerName()'\
'{return longName}return innerName()}}'
expected='function demo(a){if(a){function b(){return a}return b()}}'
assert "$expected" "$input"

input='function demo(longName){if(longName){function innerName(otherName)'\
'{return otherName}return innerName(longName)}}'
expected='function demo(a){if(a){function b(a){return a}return b(a)}}'
assert "$expected" "$input"

input='function demo(){function firstName(longName){return longName}'\
'function secondName(otherName){return otherName}'\
'return firstName(1)+secondName(2)}'
expected='function demo(){function a(a){return a}function b(a){return a}'\
'return a(1)+b(2)}'
assert "$expected" "$input"

input='function demo(longName){return ()=>longName}'
expected='function demo(longName){return()=>longName}'
assert "$expected" "$input"

input='function demo(longName){return `${longName}`}'
expected='function demo(longName){return`${longName}`}'
assert "$expected" "$input"

input='function demo(a,longName){let b=longName;return a+b}'
expected='function demo(a,c){let b=c;return a+b}'
assert "$expected" "$input"

input='function one(longName){return longName}function two(otherName)'\
'{return otherName}'
expected='function one(a){return a}function two(a){return a}'
assert "$expected" "$input"

input='let demo=function namedName(longName){return namedName(longName)}'
expected='let demo=function a(b){return a(b)}'
assert "$expected" "$input"

echo 'Passed all tests'
