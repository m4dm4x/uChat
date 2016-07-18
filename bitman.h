/* _     _ _                           _
 *| |__ (_) |_ _ __ ___   __ _ _ __   | |__
 *| '_ \| | __| '_ ` _ \ / _` | '_ \  | '_ \
 *| |_) | | |_| | | | | | (_| | | | |_| | | |
 *|_.__/|_|\__|_| |_| |_|\__,_|_| |_(_)_| |_|
 *
 * Bitmanipulation Makros
 */


#define GET_BIT(bitnumber,number) ((number>>bitnumber)&1)

/* Set-Bit Makros
 * bitnumber - eine Nummer die die Position des gesetzten Bits bezeichnet
 * number - Die Nummer in der das Bit gesetzt werden soll
 */
#define SBI(bitnumber,number) (number |= (1<<bitnumber))

/* Clear-Bit Makro 
 * bitnumber - eine Nummer die die Position des gelöschten Bits bezeichnet
 * number - Die Nummer in der das Bit gelöscht werden soll
 */
#define CBI(bitnumber,number) (number &= (~(1<<bitnumber)))
