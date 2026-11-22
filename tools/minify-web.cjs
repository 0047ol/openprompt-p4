/* Retain global names and behavior for shared Demo and controller modules. */
const fs=require('node:fs'),path=require('node:path');
let esbuild;
try{esbuild=require(require.resolve('esbuild',{paths:[__dirname,path.join(__dirname,'../app')]}));}
catch{throw Error('Install the web build dependency first: npm --prefix tools ci');}
process.stdout.write(esbuild.transformSync(fs.readFileSync(0,'utf8'),{
 loader:process.argv.includes('--css')?'css':'js',target:'es2020',charset:'utf8',minifyWhitespace:true,
 minifyIdentifiers:false,minifySyntax:false,legalComments:'inline'
}).code);
