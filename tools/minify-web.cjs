/* Retain global names and behavior for shared Demo and controller modules. */
const fs=require('node:fs'),path=require('node:path');
let esbuild;
try{esbuild=require(require.resolve('esbuild',{paths:[__dirname,path.join(__dirname,'../app')]}));}
catch{throw Error('Install the web build dependency first: npm --prefix tools ci');}
process.stdout.write(esbuild.transformSync(fs.readFileSync(0,'utf8'),{
 loader:process.argv.includes('--css')?'css':'js',target:'es2020',charset:'utf8',minifyWhitespace:true,
 // esbuild preserves top-level names without a module/output format. Only
 // function-local identifiers shrink; shared global entry points stay intact.
 minifyIdentifiers:true,minifySyntax:true,legalComments:'inline'
}).code);
