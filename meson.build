project('ablc', 'c',
        version : '1.0.0',
        default_options : ['warning_level=3', 'c_std=gnu11'])

# add_global_arguments('-Db_sanitize=address,leak,undefined', language : 'c')

sources = [
        'src/main.c',
        'src/data/abc_arr.c',
        'src/abc_lexer.c',
        'src/abc_parser.c',
        'src/abc_typechecker.c',
        'src/data/abc_pool.c'
]

#ablc = executable('ablc', sources, c_args: '-fsanitize=address,leak,undefined', link_args: '-fsanitize=address,leak,undefined')
ablc = executable('ablc', sources)
test('test', ablc)